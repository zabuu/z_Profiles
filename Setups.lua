-- Named snapshots are flushed by the DLL, independently of SavedVariables.
local Z = zProfiles

local function Hex(text)
  return string.gsub(text, ".", function(c) return string.format("%02x", string.byte(c)) end)
end
local function Unhex(text)
  if text == "" or math.mod(string.len(text), 2) ~= 0 or string.find(text, "[^0-9a-f]") then
    error("Invalid binding data.")
  end
  return string.gsub(text, "..", function(pair)
    local value = tonumber(pair, 16)
    if value == 0 then error("Invalid binding data.") end
    return string.char(value)
  end)
end

function Z:CaptureBindings()
  local keys, rows = {}, {}
  for i = 1, GetNumBindings() do
    local command, first, second = GetBinding(i)
    if command and string.sub(command, 1, 6) ~= "HEADER" then
      local bound = { first, second }
      if GetBindingKey then bound = { GetBindingKey(command) } end
      for _, key in pairs(bound) do
        if type(key) == "string" and key ~= "" and not keys[key] then
          keys[key] = true
          table.insert(rows, Hex(key) .. "\t" .. Hex(command) .. "\n")
        end
      end
    end
  end
  table.sort(rows)
  return "ZPB1\n" .. table.concat(rows)
end

function Z:ParseBindings(text)
  if type(text) ~= "string" or string.sub(text, 1, 5) ~= "ZPB1\n" or string.sub(text, -1) ~= "\n" then
    error("Invalid setup bindings.")
  end
  local rows, keys = {}, {}
  for line in string.gfind(string.sub(text, 6), "[^\n]+") do
    local _, _, key, command = string.find(line, "^([0-9a-f]+)\t([0-9a-f]+)$")
    if not key then error("Invalid setup binding.") end
    key, command = Unhex(key), Unhex(command)
    if keys[key] then error("Duplicate setup binding.") end
    keys[key] = true
    table.insert(rows, { key, command })
  end
  return rows
end

function Z:ReplaceBindings(text)
  -- Capture the full key list before unbinding: the enumeration changes as
  -- direct-action bindings disappear. Clearing first also preserves swaps.
  local target = self:ParseBindings(text)
  local previous = self:ParseBindings(self:CaptureBindings())
  for _, row in ipairs(previous) do
    if not SetBinding(row[1]) then error("Could not clear binding " .. row[1] .. ".") end
  end
  for _, row in ipairs(target) do
    if not SetBinding(row[1], row[2]) then error("Could not set binding " .. row[1] .. ". Check that its addon is enabled.") end
  end
end

function Z:SetupsReady()
  if not self:Available() or self:Call("setupVersion") ~= 1 or self:Call("liveBarsVersion") ~= 1 then
    return nil, "Install zAPI.dll 3.5+ from github.com/zabuu/zAPI beside WoW.exe, then fully restart."
  end
  local account, active, configured, problem = self:Call("status")
  if active ~= "isolated" then return nil, "Choose Isolate for this account in /zp, then fully restart WoW." end
  if problem and problem ~= "ok" then return nil, problem end
  if UnitAffectingCombat and UnitAffectingCombat("player") then return nil, "Leave combat before saving or switching setups." end
  if CursorHasItem and CursorHasItem() or CursorHasSpell and CursorHasSpell() then
    return nil, "Clear the cursor before saving or switching setups."
  end
  return true
end

function Z:SetupNotice(message)
  self.setupNotice = message
  if self.RenderSetups then self:RenderSetups() end
end

function Z:RefreshSetups()
  local ready, reason = self:SetupsReady()
  self.setupsReady, self.setupsReason = ready, reason
  self.setupNames, self.hasSetupUndo = {}, false
  if ready then
    local names, undo = self:Call("setups")
    if type(names) == "string" then
      for name in string.gfind(names, "[^\n]+") do table.insert(self.setupNames, name) end
      self.hasSetupUndo = undo == 1
    else self.setupsReady, self.setupsReason = nil, undo or "Character setups are not available yet." end
  end
  local found = false
  for _, name in ipairs(self.setupNames) do if name == self.selectedSetup then found = true end end
  if not found then self.selectedSetup = self.setupNames[1] end
  if self.RenderSetups then self:RenderSetups() end
end

function Z:SaveSetup(name, update)
  local ready, reason = self:SetupsReady()
  if not ready then self:SetupNotice(reason); return end
  local captured, bindings = pcall(function() return Z:CaptureBindings() end)
  if not captured then self:SetupNotice(tostring(bindings)); return end
  local ok, message = self:Call(update and "updateSetup" or "saveSetup", name, bindings)
  if ok == 1 then self.selectedSetup = name end
  self:RefreshSetups()
  self:SetupNotice(message)
end

function Z:ApplySetup(name, undo)
  local ready, reason = self:SetupsReady()
  if not ready then self:SetupNotice(reason); return end
  local ok, target = self:Call(undo and "setupUndoBindings" or "setupBindings", name or "")
  if ok ~= 1 then self:SetupNotice(target); return end
  local valid, errorText = pcall(function() Z:ParseBindings(target) end)
  if not valid then self:SetupNotice(tostring(errorText)); return end
  local captured, previous = pcall(function() return Z:CaptureBindings() end)
  if not captured then self:SetupNotice(tostring(previous)); return end
  local bindingSet = GetCurrentBindingSet()
  if not undo then
    local saved, message = self:Call("saveSetupUndo", "", previous)
    if saved ~= 1 then self:SetupNotice(message); return end
  end
  local changed, problem = pcall(function()
    Z:ReplaceBindings(target)
    -- Setups are character scoped. This writes only the isolated character's
    -- cache and avoids changing shared bindings for other characters.
    SaveBindings(2)
  end)
  local applied, message
  if changed then applied, message = self:Call(undo and "applySetupUndo" or "applySetup", name or "", target) end
  if not changed or applied ~= 1 then
    local restored, restoreError = pcall(function() Z:ReplaceBindings(previous); SaveBindings(bindingSet) end)
    self:RefreshSetups()
    self:SetupNotice("Could not apply setup: " .. tostring(problem or message) ..
      (restored and " Previous bindings restored." or " Binding restore failed: " .. tostring(restoreError)))
    return
  end
  if MultiActionBar_Update then MultiActionBar_Update() end
  self.lastAppliedSetup = undo and "Previous setup (Undo)" or name
  self:RefreshSetups()
  self:SetupNotice(message)
  self:Message(message)
end

function Z:RenameSetup(name)
  local ready, reason = self:SetupsReady()
  if not ready then self:SetupNotice(reason); return end
  local ok, message = self:Call("renameSetup", self.selectedSetup, name)
  if ok == 1 then self.selectedSetup = name end
  self:RefreshSetups(); self:SetupNotice(message)
end

function Z:DeleteSetup()
  local ready, reason = self:SetupsReady()
  if not ready then self:SetupNotice(reason); return end
  local ok, message = self:Call("deleteSetup", self.selectedSetup)
  self:RefreshSetups(); self:SetupNotice(message)
end
