local src, dst, chunkname = ...
local f = assert(io.open(src, "rb"))
local code = f:read("a")
f:close()
if code:sub(1, 3) == "\239\187\191" then
  code = code:sub(4)
end
local fn = assert(load(code, "@" .. chunkname))
local out = assert(io.open(dst, "wb"))
out:write(string.dump(fn))
out:close()
