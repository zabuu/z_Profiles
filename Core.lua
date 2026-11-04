-- WoW 1.12 / Lua 5.0. Protection is owned by the native layer, even if
-- this addon is disabled or no logout event is delivered.
zProfiles = { version = "0.3.1", accountPage = 1, characterPage = 1 }
local Z = zProfiles

function Z:Message(message)
  if DEFAULT_CHAT_FRAME then
    DEFAULT_CHAT_FRAME:AddMessage("|cffc77dffzProfiles:|r " .. tostring(message))
  end
end

function Z:Call(command, a, b, c)
  if type(zProfilesNative) ~= "function" then return nil, "Install zAPI.dll with profile protection, then restart WoW." end
  local success, value, second, third, fourth, fifth = pcall(zProfilesNative, command, a, b, c)
  if not success then return nil, "Native protection failed: " .. tostring(value) end
  if value == 0 and (command == "status" or command == "accounts" or command == "characters") then
    return nil, nil, nil, second
  end
  return value, second, third, fourth, fifth
end

function Z:Available()
  if type(zAPI) ~= "function" then return false end
  local ok, capability = pcall(zAPI, "profilesVersion")
  if not ok or capability ~= 1 then return false end
  local version = self:Call("version")
  return version == 1
end

function Z:Rows(text)
  local rows = {}
  if type(text) ~= "string" then return rows end
  for line in string.gfind(text, "[^\n]+") do
    local row = {}
    -- Native fields cannot contain tabs/newlines. Empty fields are not used.
    for field in string.gfind(line, "[^\t]+") do table.insert(row, field) end
    table.insert(rows, row)
  end
  return rows
end

function Z:Refresh()
  self.available = self:Available()
  self.accounts = self:Rows(self:Call("accounts"))
  self.currentAccount, self.activeMode, self.nextMode, self.problem, self.blocked = self:Call("status")
  if not self.selected then self.selected = self.currentAccount end
  if not self.selected and self.accounts[1] then self.selected = self.accounts[1][1] end
  self.characters = self:Rows(self:Call("characters", self.selected))
  if self.Render then self:Render() end
end

function Z:SelectAccount(account)
  self.selected = account
  self.characterPage = 1
  self:Refresh()
end

function Z:SetMode(mode)
  if not self.selected then return end
  local ok, message = self:Call("setMode", self.selected, mode)
  self:Message(message or "Could not change this account's mode.")
  self:Refresh()
  return ok
end

function Z:RevertCharacter(realm, character)
  if not self.selected then return end
  local ok, message, current = self:Call("revertCharacter", self.selected, realm, character)
  self:Message(message or "Could not revert this character.")
  if current == 1 then ReloadUI() else self:Refresh() end
end

function Z:RevertAccount()
  if not self.selected then return end
  local ok, message, current = self:Call("revertAccount", self.selected)
  self:Message(message or "Could not revert this account.")
  if current == 1 then ReloadUI() else self:Refresh() end
end

local events = CreateFrame("Frame")
events:RegisterEvent("PLAYER_LOGIN")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:SetScript("OnEvent", function()
  if not Z:Available() then return end
  if event == "PLAYER_LOGIN" or event == "PLAYER_ENTERING_WORLD" then
    Z:Call("identify", GetRealmName(), UnitName("player"))
    Z:Call("resume")
    local account, active, configured, problem = Z:Call("status")
    if active == "isolated" and MultiActionBar_Update then MultiActionBar_Update() end
    if problem and problem ~= "ok" then Z:Message(problem) end
    if not Z.announced then
      Z.announced = true
      if active == "isolated" then
        Z:Message("Isolation active for " .. tostring(account) .. ". /zp to manage profiles.")
      end
      if configured and active ~= configured then Z:Message("Account mode changes take effect after a full client restart.") end
    end
    if Z.window and Z.window:IsShown() then Z:Refresh() end
  end
end)

SLASH_ZPROFILES1 = "/zp"
SLASH_ZPROFILES2 = "/zprofiles"
SlashCmdList["ZPROFILES"] = function(message)
  if message and string.find(string.lower(message), "^%s*bars%s*$") then Z:ToggleSetups()
  else Z:Toggle() end
end
