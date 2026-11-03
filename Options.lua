local Z = zProfiles
local purple = { 0.78, 0.52, 1 }

local function Label(parent, text, size, x, y, width)
  local label = parent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
  label:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y)
  label:SetFont("Fonts\\FRIZQT__.TTF", size)
  label:SetJustifyH("LEFT")
  if width then label:SetWidth(width) end
  label:SetText(text)
  return label
end

local function Button(parent, text, width, x, y, onClick)
  local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
  button:SetWidth(width)
  button:SetHeight(24)
  button:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y)
  button:SetText(text)
  button:SetScript("OnClick", onClick)
  return button
end

local function Enabled(button, enabled)
  if enabled then button:Enable() else button:Disable() end
end

function Z:CreateWindow()
  local window = CreateFrame("Frame", "zProfilesWindow", UIParent)
  self.window = window
  window:SetWidth(790)
  window:SetHeight(510)
  window:SetPoint("CENTER", UIParent, "CENTER", 0, 0)
  window:SetFrameStrata("DIALOG")
  window:SetMovable(true)
  window:EnableMouse(true)
  window:SetClampedToScreen(true)
  window:RegisterForDrag("LeftButton")
  window:SetScript("OnDragStart", function() this:StartMoving() end)
  window:SetScript("OnDragStop", function() this:StopMovingOrSizing() end)
  window:SetBackdrop({ bgFile = "Interface\\Tooltips\\UI-Tooltip-Background", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", tile = true, tileSize = 16, edgeSize = 16, insets = { left = 5, right = 5, top = 5, bottom = 5 } })
  window:SetBackdropColor(0.055, 0.045, 0.075, 0.98)
  window:SetBackdropBorderColor(purple[1], purple[2], purple[3], 0.8)
  Label(window, "|cffc77dffz|rProfiles", 22, 22, -20)
  self.statusLabel = Label(window, "", 12, 24, -55, 740)
  self.noticeLabel = Label(window, "", 11, 24, -82, 740)
  Label(window, "ACCOUNTS ON THIS COMPUTER", 11, 24, -122)
  self.accountTitle = Label(window, "Select an account", 15, 302, -120, 460)
  Button(window, "X", 26, 744, -18, function() window:Hide() end)
  Button(window, "Bar setups", 100, 574, -82, function() Z:ToggleSetups() end)
  Button(window, "Refresh", 80, 680, -82, function() Z:Refresh() end)

  self.accountButtons = {}
  for i = 1, 9 do
    local button = Button(window, "", 252, 24, -146 - (i - 1) * 29, function() Z:SelectAccount(this.account) end)
    self.accountButtons[i] = button
  end
  self.previousAccounts = Button(window, "<", 36, 24, -414, function() Z.accountPage = math.max(1, Z.accountPage - 1); Z:Render() end)
  self.accountPageLabel = Label(window, "", 11, 75, -421, 140)
  self.nextAccounts = Button(window, ">", 36, 240, -414, function() Z.accountPage = Z.accountPage + 1; Z:Render() end)

  self.defaultButton = Button(window, "Use Default", 125, 302, -149, function() Z:SetMode("default") end)
  self.isolateButton = Button(window, "Isolate", 125, 433, -149, function() Z:SetMode("isolated") end)
  self.revertAccountButton = Button(window, "Revert account", 170, 572, -149, function() Z:RevertAccount() end)
  self.accountModeLabel = Label(window, "", 11, 302, -181, 445)
  Label(window, "CHARACTER", 11, 302, -216)
  Label(window, "REALM", 11, 477, -216)
  self.characterLabels, self.realmLabels, self.revertButtons = {}, {}, {}
  for i = 1, 5 do
    self.characterLabels[i] = Label(window, "", 12, 302, -246 - (i - 1) * 31, 170)
    self.realmLabels[i] = Label(window, "", 11, 477, -247 - (i - 1) * 31, 162)
    self.revertButtons[i] = Button(window, "Revert", 96, 646, -240 - (i - 1) * 31, function()
      Z:RevertCharacter(this.realm, this.character)
    end)
  end
  self.emptyLabel = Label(window, "", 12, 302, -260, 430)
  self.previousCharacters = Button(window, "<", 36, 302, -414, function() Z.characterPage = math.max(1, Z.characterPage - 1); Z:Render() end)
  self.characterPageLabel = Label(window, "", 11, 355, -421, 270)
  self.nextCharacters = Button(window, ">", 36, 706, -414, function() Z.characterPage = Z.characterPage + 1; Z:Render() end)
  Label(window, "Character Revert restores its files and action bars. Shared macros and bindings: Revert account.", 11, 24, -457, 738)
  Label(window, "Reverts keep a local backup. The current character reloads its UI. Default accounts are left alone.", 11, 24, -478, 738)
  if UISpecialFrames then table.insert(UISpecialFrames, "zProfilesWindow") end
  window:Hide()
end

function Z:Render()
  if not self.window then return end
  local accounts = self.accounts or {}
  local characters = self.characters or {}
  local accountPages = math.max(1, math.ceil(table.getn(accounts) / 9))
  local characterPages = math.max(1, math.ceil(table.getn(characters) / 5))
  self.accountPage = math.min(self.accountPage, accountPages)
  self.characterPage = math.min(self.characterPage, characterPages)
  local selected
  for _, row in ipairs(accounts) do if row[1] == self.selected then selected = row end end
  if not self.available then
    self.statusLabel:SetText("|cffffaa55Native protection is not loaded.|r")
    self.noticeLabel:SetText("Install the bundled native layer, then restart WoW. Profile controls are unavailable.")
  elseif self.problem and self.problem ~= "ok" then
    self.statusLabel:SetText("|cffff7777" .. self.problem .. "|r")
    self.noticeLabel:SetText("Check the native log before making settings changes.")
  else
    local mode = self.activeMode == "isolated" and "|cff77dd99Isolated|r" or "Default"
    self.statusLabel:SetText("Current account: " .. tostring(self.currentAccount or "unknown") .. "   |   " .. mode)
    self.noticeLabel:SetText("Choose a mode per account. Mode changes require a full client restart.")
  end
  self.accountTitle:SetText(self.selected or "Select an account")
  for i, button in ipairs(self.accountButtons) do
    local row = accounts[(self.accountPage - 1) * 9 + i]
    if row then
      button.account = row[1]
      local color = row[1] == self.selected and "|cffc77dff" or "|cffffffff"
      button:SetText(color .. row[1] .. "|r  -  " .. (row[3] == "isolated" and "Isolated" or "Default"))
      button:Show()
    else button:Hide() end
  end
  self.accountPageLabel:SetText(self.accountPage .. " / " .. accountPages)
  self.characterPageLabel:SetText("Characters  " .. self.characterPage .. " / " .. characterPages)
  Enabled(self.previousAccounts, self.accountPage > 1)
  Enabled(self.nextAccounts, self.accountPage < accountPages)
  Enabled(self.previousCharacters, self.characterPage > 1)
  Enabled(self.nextCharacters, self.characterPage < characterPages)
  local usable = self.available and selected
  Enabled(self.defaultButton, usable and selected[3] ~= "default")
  Enabled(self.isolateButton, usable and selected[3] ~= "isolated")
  local isolated = usable and selected[2] == "isolated"
  Enabled(self.revertAccountButton, isolated)
  if selected then
    local text = "Active: " .. selected[2] .. "."
    if selected[2] ~= selected[3] then text = text .. " After restart: " .. selected[3] .. "." end
    self.accountModeLabel:SetText(text)
  else self.accountModeLabel:SetText("Unlisted accounts use Default automatically.") end
  self.emptyLabel:SetText(table.getn(characters) == 0 and "No local character folders found." or "")
  for i, button in ipairs(self.revertButtons) do
    local row = characters[(self.characterPage - 1) * 5 + i]
    if row then
      self.characterLabels[i]:SetText(row[2] .. (row[3] == "1" and " |cff77dd99(online)|r" or ""))
      self.realmLabels[i]:SetText(row[1])
      button.realm, button.character = row[1], row[2]
      button:Show()
      Enabled(button, isolated)
    else
      self.characterLabels[i]:SetText("")
      self.realmLabels[i]:SetText("")
      button:Hide()
    end
  end
end

function Z:Toggle()
  if not self.window then self:CreateWindow() end
  if self.window:IsShown() then self.window:Hide() else self:Refresh(); self.window:Show() end
end
