
import json

fp = "/Users/thakeenathees/Desktop/thakee/temp/lsp/main.c"

method = "textDocument/definition"
params = {
        "textDocument" : {
            "uri" : "file://" + fp,
        },
        "position" : {
            "line": 3,
            "character" : 2,
            }
}

# content = ""
# with open(fp, 'r') as f:
#     content = f.read()
# method = "textDocument/didOpen"
# params = {
#         "textDocument" : {
#             "uri" : "file://" + fp,
#             "text" : content,
#             "languageId" : "c",
#             }
#         }

data = {
        "id" : 2,
        "jsonrpc" : "2.0",
        "method" : method,
        "params" : params,

        }

j_data = json.dumps(data)
send_data = f"Content-Length: {len(j_data)}\r\n\r\n{j_data}"
print(send_data.__repr__().replace('"', '\\"').replace("'", '"'))
