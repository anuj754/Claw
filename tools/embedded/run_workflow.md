# run_workflow

Execute a workflow by its ID. Optionally provide input variables that can be referenced in steps via {{variable}} syntax. Steps are executed sequentially and each step's result is stored in its output_var.

**Category**: workflow

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| workflow_id | string | yes | The workflow ID to execute |
| input_vars | object | no | Input variables (key-value pairs) available to all workflow steps |

## Schema

```json
{
  "name": "run_workflow",
  "description": "Execute a workflow by its ID. Optionally provide input variables that can be referenced in steps via {{variable}} syntax.",
  "inputSchema": {
    "type": "object",
    "properties": {
      "workflow_id": {
        "type": "string",
        "description": "The workflow ID to execute"
      },
      "input_vars": {
        "type": "object",
        "description": "Input variables (key-value pairs) available to all workflow steps"
      }
    },
    "required": ["workflow_id"]
  }
}
```
