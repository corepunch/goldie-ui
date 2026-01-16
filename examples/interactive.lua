-- Interactive Lua script demonstrating io.read with coroutines

print("What is your name?")
local name = io.read()

print("Hello, " .. name .. "!")
print("How old are you?")
local age = io.read()

print("Nice to meet you, " .. name .. "!")
print("You are " .. age .. " years old.")
print("\nScript finished!")
