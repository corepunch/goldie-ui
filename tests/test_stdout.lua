-- Test script to verify stdout interception
-- This should test both print() and io.write()

print("Testing print() function")
io.write("Testing io.write() function\n")
io.stdout:write("Testing io.stdout:write() function\n")

-- Test that file I/O still works
local f = io.open("/tmp/test_output.txt", "w")
if f then
  f:write("This should go to a file\n")
  f:close()
  print("File write successful")
else
  print("File write failed")
end
