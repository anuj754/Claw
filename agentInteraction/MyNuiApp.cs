// MyNuiApp.cs — Full NUI app
// - TizenClawRawClient  → sends prompts via \0tizenclaw.sock (no libtizenclaw.so)
// - CanvasStateClient   → receives state events via /run/tizenclaw/canvas.sock

using System;
using System.Threading;
using System.Threading.Tasks;
using Tizen.NUI;
using Tizen.NUI.BaseComponents;

namespace MyNuiApp
{
    class Program : NUIApplication
    {
        private TizenClawRawClient _agent;
        private CanvasStateClient  _canvas;

        private TextLabel  _statusLabel;
        private TextLabel  _toolResultLabel;
        private TextLabel  _responseLabel;
        private TextField  _inputField;
        private Button     _sendButton;

        private CancellationTokenSource _requestCts;

        protected override void OnCreate()
        {
            base.OnCreate();
            BuildUi();
            InitCanvas();
        }

        // -------------------------------------------------------
        // UI layout
        // -------------------------------------------------------
        private void BuildUi()
        {
            var window = Window.Instance;
            window.BackgroundColor = Color.White;

            var root = new View
            {
                Layout = new LinearLayout
                {
                    LinearOrientation = LinearLayout.Orientation.Vertical,
                    CellPadding       = new Size2D(0, 16),
                },
                WidthSpecification  = LayoutParamPolicies.MatchParent,
                HeightSpecification = LayoutParamPolicies.MatchParent,
                Padding             = new Extents(24, 24, 48, 24),
            };
            window.Add(root);

            // Status bar — shows canvas socket state events
            _statusLabel = new TextLabel("Initializing...")
            {
                PointSize          = 11,
                TextColor          = new Color(0.4f, 0.4f, 0.4f, 1),
                WidthSpecification = LayoutParamPolicies.MatchParent,
            };
            root.Add(_statusLabel);

            // Tool result area — shows the last tool execution output
            _toolResultLabel = new TextLabel("")
            {
                MultiLine           = true,
                PointSize           = 11,
                TextColor           = new Color(0.1f, 0.4f, 0.1f, 1),
                WidthSpecification  = LayoutParamPolicies.MatchParent,
                HeightSpecification = 80,
                Visibility          = false,
            };
            root.Add(_toolResultLabel);

            // Response display
            _responseLabel = new TextLabel("")
            {
                MultiLine           = true,
                PointSize           = 13,
                TextColor           = Color.Black,
                WidthSpecification  = LayoutParamPolicies.MatchParent,
                HeightSpecification = 300,
            };
            root.Add(_responseLabel);

            // Input row
            var inputRow = new View
            {
                Layout = new LinearLayout
                {
                    LinearOrientation = LinearLayout.Orientation.Horizontal,
                    CellPadding       = new Size2D(8, 0),
                },
                WidthSpecification = LayoutParamPolicies.MatchParent,
            };
            root.Add(inputRow);

            _inputField = new TextField
            {
                PlaceholderText    = "Type a command...",
                PointSize          = 13,
                WidthSpecification = LayoutParamPolicies.WrapContent,
                Weight             = 1,
            };
            inputRow.Add(_inputField);

            _sendButton = new Button { Text = "Send", PointSize = 13 };
            _sendButton.Clicked += OnSendClicked;
            inputRow.Add(_sendButton);

            // Cancel button — stops an in-progress stream
            var cancelButton = new Button
            {
                Text      = "Cancel",
                PointSize = 13,
                IsEnabled = false,
            };
            cancelButton.Clicked += (_, _) => _requestCts?.Cancel();
            inputRow.Add(cancelButton);

            // Keep reference so we can enable/disable it
            _sendButton.Name   = "send";
            cancelButton.Name  = "cancel";
        }

        // -------------------------------------------------------
        // Connect canvas state socket on startup
        // -------------------------------------------------------
        private async void InitCanvas()
        {
            _agent = new TizenClawRawClient();
            SetStatus("Agent ready");

            _canvas = new CanvasStateClient();
            _canvas.OnStateChanged += ev =>
            {
                // Canvas fires on background thread — marshal to NUI main thread
                _ = MainThread.InvokeOnMainThreadAsync(() =>
                {
                    switch (ev.State)
                    {
                        case AgentState.Thinking:
                            SetStatus("Thinking...");
                            // Clear tool result area when a new request starts
                            SetToolResult("");
                            break;
                        case AgentState.ToolCall:
                            SetStatus(ev.Content); // "Executing N tools..."
                            break;
                        case AgentState.Idle:
                            SetStatus("Ready");
                            break;
                        default:
                            SetStatus(ev.Content);
                            break;
                    }
                });
            };
            _canvas.OnToolResult += ev =>
            {
                _ = MainThread.InvokeOnMainThreadAsync(() =>
                {
                    // Show a preview of the tool result (first 200 chars)
                    string preview = ev.ResultJson.Length > 200
                        ? ev.ResultJson.Substring(0, 200) + "…"
                        : ev.ResultJson;
                    SetToolResult($"[{ev.ToolName}]\n{preview}");
                    SetStatus($"Tool done: {ev.ToolName}");
                });
            };
            _canvas.OnDisconnected += () =>
            {
                _ = MainThread.InvokeOnMainThreadAsync(
                    () => SetStatus("Canvas disconnected"));
            };

            bool ok = await _canvas.ConnectAsync();
            if (!ok)
                SetStatus("Canvas unavailable — state events disabled");
        }

        // -------------------------------------------------------
        // Send button — streaming request, no library
        // -------------------------------------------------------
        private async void OnSendClicked(object sender, EventArgs e)
        {
            string prompt = _inputField.Text.Trim();
            if (string.IsNullOrEmpty(prompt)) return;

            _inputField.Text      = "";
            _sendButton.IsEnabled = false;
            _responseLabel.Text   = "";

            _requestCts = new CancellationTokenSource();
            var ct = _requestCts.Token;

            try
            {
                // Speak directly to \0tizenclaw.sock — no libtizenclaw.so
                await _agent.SendRequestStreamAsync(
                    sessionId: "nui_session",
                    prompt:    prompt,
                    onChunk: (chunk, isDone) =>
                    {
                        // onChunk fires on background thread
                        _ = MainThread.InvokeOnMainThreadAsync(() =>
                        {
                            _responseLabel.Text += chunk;

                            if (isDone)
                            {
                                _sendButton.IsEnabled = true;
                                SetStatus("Ready");
                            }
                        });
                    },
                    ct: ct);
            }
            catch (OperationCanceledException)
            {
                SetStatus("Cancelled");
                _sendButton.IsEnabled = true;
            }
            catch (Exception ex)
            {
                // UID not in {0,200,301,5001} → Connection refused
                // Daemon not running → Connection refused
                _responseLabel.Text   = $"Error: {ex.Message}";
                _sendButton.IsEnabled = true;
                SetStatus("Error");

                Tizen.Log.Error("MYAPP", $"SendRequest failed: {ex}");
            }
            finally
            {
                _requestCts?.Dispose();
                _requestCts = null;
            }
        }

        private void SetStatus(string text)
        {
            if (_statusLabel != null)
                _statusLabel.Text = text;
        }

        private void SetToolResult(string text)
        {
            if (_toolResultLabel == null) return;
            _toolResultLabel.Text       = text;
            _toolResultLabel.Visibility = !string.IsNullOrEmpty(text);
        }

        protected override void OnTerminate()
        {
            _requestCts?.Cancel();
            _canvas?.Dispose();
            _agent?.Dispose();
            base.OnTerminate();
        }

        static void Main(string[] args) => new Program().Run(args);
    }
}
