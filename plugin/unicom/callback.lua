local cjson = require 'cjson.safe'

local M = {}

--- TODO: add your handler function here
function M.handler(args)
    if args.topic == 'report_timer' then
        return cjson.encode(args.data)
    end
end

return M