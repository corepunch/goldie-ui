-- Test script that generates long output to test terminal scrolling
print("Terminal Scrolling Test")
print("====================")
print("")

for i = 1, 50 do
  print("Line " .. i .. ": This is a test line to demonstrate text wrapping and scrolling in the terminal view. It should wrap nicely when the line is too long.")
end

print("")
print("End of output - you should be able to scroll through all lines!")
