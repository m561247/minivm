local upto = 1000000
local count = 1
local val = 3
while val < upto do
    local check = 3
    local add = 1
    while check * check <= val do
        if val % check == 0 then
            add = 0
        end
        check = check + 2
    end
    count = count + add
    val = val + 2
end
print(count)
