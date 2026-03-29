# delete_workflow

Delete a workflow by its ID. The workflow definition file will be removed from disk.

**Category**: workflow

## Parameters

| Name | Type | Required | Description |
|------|------|----------|-------------|
| workflow_id | string | yes | The workflow ID to delete |

## Schema

```json
{
  "name": "delete_workflow",
  "description": "Delete a workflow by its ID.",
  "inputSchema": {
    "type": "object",
    "properties": {
      "workflow_id": {
        "type": "string",
        "description": "The workflow ID to delete"
      }
    },
    "required": ["workflow_id"]
  }
}
```
