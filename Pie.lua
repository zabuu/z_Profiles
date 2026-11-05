-- Ring assignments only. Snapshot data is parsed, never run as Lua.
local Z = zProfiles
local kinds = { SPELL_OR_ITEM = true, MACRO = true, SUPER_MACRO = true, RAID_MARKER = true }

local function Integer(value, maximum)
  return type(value) == "number" and value >= 0 and value <= maximum and value == math.floor(value)
end
local function Hex(value)
  if type(value) ~= "string" or string.len(value) > 1024 or string.find(value, "%z") then
    error("Invalid zPie slot text.")
  end
  return string.gsub(value, ".", function(c) return string.format("%02x", string.byte(c)) end)
end
local function Unhex(value)
  if string.len(value) > 2048 or math.mod(string.len(value), 2) ~= 0 or string.find(value, "[^0-9a-f]") then
    error("Invalid zPie slot text.")
  end
  local result = string.gsub(value, "..", function(pair)
    local byte = tonumber(pair, 16)
    if byte == 0 then error("Invalid zPie slot text.") end
    return string.char(byte)
  end)
  return result
end
local function Ready()
  if not zPie or type(zPieDB) ~= "table" or type(zPie.GetRingCount) ~= "function" or
     type(zPie.CreateRingData) ~= "function" or type(zPie.InitButtons) ~= "function" or
     type(zPie.RefreshDBIcons) ~= "function" then
    error("Enable zPie and finish logging in before applying a setup with ring slots.")
  end
  if zPie.currentRing or zPie.pendingItem then error("Close the zPie ring and finish its action before switching setups.") end
end

function Z:ParsePie(text)
  if text == nil or text == "" then return nil end
  if type(text) ~= "string" or string.len(text) > 1024 * 1024 or string.sub(text, -1) ~= "\n" then
    error("Invalid zPie setup data.")
  end
  local _, last, count = string.find(text, "^ZPR1\n(%d+)\n")
  count = tonumber(count)
  if not count or not Integer(count, 256) or count < 1 then error("Invalid zPie ring count.") end
  local snapshot = { count = count, rings = {} }
  for i = 1, count do snapshot.rings[i] = {} end
  for row in string.gfind(string.sub(text, last + 1), "([^\n]*)\n") do
    local _, _, ring, slot, kind, name, icon, macro, marker = string.find(row,
      "^(%d+)\t(%d+)\t([A-Z_]+)\t([0-9a-f]+)\t([0-9a-f]*)\t(%d+)\t(%d+)$")
    ring, slot, macro, marker = tonumber(ring), tonumber(slot), tonumber(macro), tonumber(marker)
    if not ring or not Integer(ring, count) or ring < 1 or not slot or not Integer(slot, 8) or slot < 1 or
       not kinds[kind] or not Integer(macro, 2147483647) or not Integer(marker, 8) or
       (kind == "RAID_MARKER" and marker == 0) or (kind ~= "RAID_MARKER" and marker ~= 0) or
       snapshot.rings[ring][slot] then error("Invalid or duplicate zPie ring slot.") end
    name, icon = Unhex(name), Unhex(icon)
    snapshot.rings[ring][slot] = { name = name, type = kind, icon = icon ~= "" and icon or nil,
      macroIndex = macro > 0 and macro or nil, markerIndex = marker > 0 and marker or nil }
  end
  return snapshot
end

function Z:CapturePie()
  if not zPie then return "" end
  Ready()
  local count, rows = zPie:GetRingCount(), {}
  if not Integer(count, 256) or count < 1 then error("zProfiles supports up to 256 zPie rings per setup.") end
  for ring = 1, count do
    local items = zPieDB[ring] and zPieDB[ring].items or {}
    for slot = 1, 8 do
      local item = items[slot]
      if item and item.name and item.name ~= "" then
        local kind, macro, marker = item.type or "SPELL_OR_ITEM", item.macroIndex or 0, item.markerIndex or 0
        if not kinds[kind] or not Integer(macro, 2147483647) or not Integer(marker, 8) then
          error("Invalid zPie ring slot.")
        end
        table.insert(rows, ring .. "\t" .. slot .. "\t" .. kind .. "\t" .. Hex(item.name) .. "\t" ..
          Hex(item.icon or "") .. "\t" .. macro .. "\t" .. marker .. "\n")
      end
    end
  end
  local text = "ZPR1\n" .. count .. "\n" .. table.concat(rows)
  self:ParsePie(text)
  return text
end

function Z:CheckPie(snapshot)
  if snapshot then Ready() end
end

function Z:ReplacePie(snapshot)
  if not snapshot then return end -- Old setups and saves without zPie leave it alone.
  Ready()
  local count = math.max(zPie:GetRingCount(), snapshot.count)
  if not Integer(count, 256) then error("Invalid zPie ring count.") end
  local replacements = {}
  for ring = 1, count do
    replacements[ring] = {}
    for slot, item in pairs(snapshot.rings[ring] or {}) do
      local copy = {}
      for key, value in pairs(item) do copy[key] = value end
      -- Macro indices can move. Resolve the saved name rather than allowing
      -- icon refresh to silently rename this slot to an unrelated macro.
      if copy.type == "MACRO" or copy.type == "SUPER_MACRO" then
        local index = GetMacroIndexByName(copy.name)
        copy.macroIndex = index and index > 0 and index or nil
      end
      replacements[ring][slot] = copy
    end
  end
  for ring = 1, count do
    if not zPieDB[ring] then zPieDB[ring] = zPie:CreateRingData(ring) end
    zPieDB[ring].items = replacements[ring]
  end
  zPieDB.ringCount = count
  zPie:InitButtons()
  zPie:RefreshDBIcons()
  if zPieConfigFrame and type(zPieConfigFrame.Refresh) == "function" then zPieConfigFrame:Refresh() end
end
