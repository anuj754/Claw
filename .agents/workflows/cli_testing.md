---
description: CLI Functional Testing via tizenclaw-cli
---

# TizenClaw CLI Functional Testing

Use `tizenclaw-cli` to send natural language prompts to the running TizenClaw daemon and verify that features work end-to-end through the LLM agentic loop.

## Prerequisites
- TizenClaw is deployed and running on the device (`./deploy.sh`)
- The service is `active (running)`: `sdb shell systemctl status tizenclaw -l`
- LLM backend is configured with a valid API key

## Usage

### Single-shot Mode
Send a prompt and receive the response:
```bash
sdb shell tizenclaw-cli "your prompt here"
```

### With Session ID
Maintain conversation context across multiple calls:
```bash
sdb shell tizenclaw-cli -s my_session "first prompt"
sdb shell tizenclaw-cli -s my_session "follow-up prompt"
```

### Streaming Mode
Receive response tokens as they arrive:
```bash
sdb shell tizenclaw-cli --stream "Tell me about Tizen"
```

### Interactive Mode
Enter a REPL-style session (Ctrl+D to exit):
```bash
sdb shell tizenclaw-cli
```

## Verification Patterns

### Tool Invocation Test
Verify that the LLM correctly invokes a built-in tool:
```bash
sdb shell tizenclaw-cli "Use the list_workflows tool to show the list of registered workflows"
```

### Workflow CRUD Test
Create, list, and delete a workflow end-to-end:
```bash
# Create
sdb shell tizenclaw-cli -s wf_test "Use the create_workflow tool to create the following workflow:
---
name: Test Workflow
description: Simple test
trigger: manual
---
## Step 1: Hello
- type: prompt
- instruction: Say hello
- output_var: greeting"

# List
sdb shell tizenclaw-cli -s wf_test "Show the workflow list"

# Delete
sdb shell tizenclaw-cli -s wf_test "Delete the workflow that was just created"
```

### Log Cross-Check
After running CLI commands, cross-check daemon logs:
```bash
sdb shell dlogutil -d TIZENCLAW | grep -i "workflow\|tool\|ProcessPrompt" | tail -20
```

> [!TIP]
> The `tizenclaw-cli` binary is automatically installed as part of the `tizenclaw` RPM package. No separate installation is needed.
