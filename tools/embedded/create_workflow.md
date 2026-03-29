# create_workflow

Create a workflow from Markdown text. The markdown must include YAML frontmatter (---) with a 'name' field and '## Step N:' sections defining sequential steps. Steps execute in order with {{variable}} interpolation.

**Category**: workflow

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| markdown | string | yes | Markdown text with YAML frontmatter and Step sections |

## Markdown Format

```markdown
---
name: Workflow Name
description: What this workflow does
trigger: manual
---

## Step 1: Step Title
- type: prompt
- instruction: Tell the LLM what to do
- output_var: result_variable

## Step 2: Another Step
- type: tool
- tool_name: get_device_info
- output_var: device_info

## Step 3: Final Step
- type: prompt
- instruction: Summarize {{result_variable}} and {{device_info}}
- output_var: summary
```

## Step Metadata

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| type | string | yes | `prompt` (LLM instruction) or `tool` (direct tool call) |
| instruction | string | no | LLM prompt text (for type=prompt) |
| tool_name | string | no | Tool to invoke (for type=tool) |
| args | JSON | no | Tool arguments (for type=tool) |
| output_var | string | no | Variable name for step output |
| skip_on_failure | boolean | no | Continue on error (default: false) |
| max_retries | integer | no | Max retry count (default: 0) |

## Schema

```json
{
  "name": "create_workflow",
  "description": "Create a workflow from Markdown text with YAML frontmatter and sequential Step sections.",
  "inputSchema": {
    "type": "object",
    "properties": {
      "markdown": {
        "type": "string",
        "description": "Markdown text with YAML frontmatter and Step sections"
      }
    },
    "required": ["markdown"]
  }
}
```
