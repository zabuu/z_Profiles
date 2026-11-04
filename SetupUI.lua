local Z = zProfiles
local function Label(parent, text, size, x, y, width)
  local label = parent:CreateFontString(nil, "OVERLAY", "GameFontNormal")
  label:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y)
  label:SetFont("Fonts\\FRIZQT__.TTF", size)
  label:SetJustifyH("LEFT")
  if width then label:SetWidth(width) end
  label:SetText(text)
  return label
end
local function Button(parent, text, width, x, y, click)
  local button = CreateFrame("Button", nil, parent, "UIPanelButtonTemplate")
  button:SetWidth(width); button:SetHeight(25)
  button:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y)
  button:SetText(text); button:SetScript("OnClick", click)
  return button
end
local function Enabled(button, enabled)
  if enabled then button:Enable() else button:Disable() end
end

function Z:ConfirmSetupAction(kind)
  if not self.selectedSetup then return end
  if self.setupConfirmation == kind .. ":" .. self.selectedSetup then
    self.setupConfirmation = nil
    if kind == "update" then self:SaveSetup(self.selectedSetup, true) else self:DeleteSetup() end
  else
    self.setupConfirmation = kind .. ":" .. self.selectedSetup
    self:SetupNotice((kind == "update" and "Replace \"" or "Delete \"") .. self.selectedSetup .. "\"? Click Confirm again.")
  end
end

function Z:CreateSetupWindow()
  local window = CreateFrame("Frame", "zProfilesSetupWindow", UIParent)
  self.setupWindow = window
  window:SetWidth(370); window:SetHeight(450)
  window:SetFrameStrata("DIALOG")
  window:SetMovable(true); window:EnableMouse(true); window:SetClampedToScreen(true)
  window:RegisterForDrag("LeftButton")
  window:SetScript("OnDragStart", function()
    local left, top = this:GetLeft(), this:GetTop()
    this:SetParent(UIParent)
    this:ClearAllPoints()
    this:SetPoint("TOPLEFT", UIParent, "BOTTOMLEFT", left, top)
    this:StartMoving()
    Z:UpdateTalentSetupButton()
  end)
  window:SetScript("OnDragStop", function() this:StopMovingOrSizing() end)
  window:SetBackdrop({ bgFile = "Interface\\Tooltips\\UI-Tooltip-Background", edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border", tile = true, tileSize = 16, edgeSize = 16, insets = { left = 5, right = 5, top = 5, bottom = 5 } })
  window:SetBackdropColor(0.055, 0.045, 0.075, 0.98)
  window:SetBackdropBorderColor(0.78, 0.52, 1, 0.8)
  Label(window, "|cffc77dffz|rProfiles |cffffffff/ Bar setups|r", 17, 18, -18, 306)
  Button(window, "X", 26, 326, -14, function() window:Hide() end)
  self.setupCharacterLabel = Label(window, "", 11, 20, -47, 330)
  self.setupLastLabel = Label(window, "", 11, 20, -66, 330)
  self.setupStatusLabel = Label(window, "", 11, 20, -88, 330)
  self.setupStatusLabel:SetHeight(38)
  self.setupRows = {}
  for i = 1, 5 do
    self.setupRows[i] = Button(window, "", 250, 18, -133 - (i - 1) * 30, function()
      Z.selectedSetup = this.setupName
      Z.setupConfirmation, Z.setupNotice = nil, nil
      Z:RenderSetups()
    end)
    local apply = Button(window, "Apply", 76, 276, -133 - (i - 1) * 30, function()
      Z.setupConfirmation = nil
      Z:ApplySetup(this.setupName)
    end)
    self.setupRows[i].apply = apply
  end
  self.setupPrevious = Button(window, "<", 34, 18, -286, function()
    Z.setupPage = math.max(1, (Z.setupPage or 1) - 1); Z:RenderSetups()
  end)
  self.setupPageLabel = Label(window, "", 11, 68, -293, 220)
  self.setupNext = Button(window, ">", 34, 318, -286, function() Z.setupPage = (Z.setupPage or 1) + 1; Z:RenderSetups() end)
  self.setupUpdate = Button(window, "Update selected", 162, 18, -320, function() Z:ConfirmSetupAction("update") end)
  self.setupDelete = Button(window, "Delete selected", 164, 188, -320, function() Z:ConfirmSetupAction("delete") end)
  Label(window, "SETUP NAME", 10, 20, -360, 330)
  local edit = CreateFrame("EditBox", "zProfilesSetupName", window, "InputBoxTemplate")
  self.setupNameInput = edit
  edit:SetWidth(320); edit:SetHeight(24); edit:SetPoint("TOPLEFT", window, "TOPLEFT", 26, -376)
  edit:SetAutoFocus(false); edit:SetMaxLetters(32)
  edit:SetScript("OnEscapePressed", function() this:ClearFocus() end)
  edit:SetScript("OnEnterPressed", function() Z:SaveSetup(this:GetText()); this:ClearFocus() end)
  self.setupSave = Button(window, "Save new", 104, 18, -412, function()
    edit:ClearFocus(); Z.setupConfirmation = nil; Z:SaveSetup(edit:GetText())
  end)
  self.setupRename = Button(window, "Rename", 104, 132, -412, function()
    edit:ClearFocus(); Z.setupConfirmation = nil; Z:RenameSetup(edit:GetText())
  end)
  self.setupUndo = Button(window, "Undo switch", 106, 246, -412, function()
    Z.setupConfirmation = nil; Z:ApplySetup(nil, true)
  end)
  window:SetScript("OnShow", function()
    Z.setupNotice = nil; Z.setupConfirmation = nil; Z:RefreshSetups()
    Z:UpdateTalentSetupButton()
  end)
  window:SetScript("OnHide", function() Z:UpdateTalentSetupButton() end)
  if UISpecialFrames then table.insert(UISpecialFrames, "zProfilesSetupWindow") end
  window:Hide()
end

function Z:RenderSetups()
  if not self.setupWindow then return end
  local names = self.setupNames or {}
  local pages = math.max(1, math.ceil(table.getn(names) / 5))
  self.setupPage = math.min(self.setupPage or 1, pages)
  self.setupCharacterLabel:SetText(tostring(UnitName("player")) .. "  /  " .. tostring(GetRealmName()))
  self.setupLastLabel:SetText(self.lastAppliedSetup and "Last applied: " .. self.lastAppliedSetup or "Change talents first, then apply the matching setup.")
  self.setupStatusLabel:SetText(self.setupsReason or self.setupNotice or "Save bindings, all 120 slots and stock bar visibility.\nSelect a setup to update or rename it.")
  local ready = self.setupsReady
  for i, row in ipairs(self.setupRows) do
    local name = names[(self.setupPage - 1) * 5 + i]
    row.setupName, row.apply.setupName = name, name
    if name then
      row:SetText((self.selectedSetup == name and "|cffc77dff" or "|cffffffff") .. name .. "|r")
      row:Show(); row.apply:Show(); Enabled(row.apply, ready)
    else row:Hide(); row.apply:Hide() end
  end
  self.setupPageLabel:SetText(table.getn(names) == 0 and "No saved setups yet" or "Setups  " .. self.setupPage .. " / " .. pages)
  Enabled(self.setupPrevious, self.setupPage > 1); Enabled(self.setupNext, self.setupPage < pages)
  Enabled(self.setupSave, ready)
  Enabled(self.setupUpdate, ready and self.selectedSetup)
  Enabled(self.setupDelete, ready and self.selectedSetup)
  Enabled(self.setupRename, ready and self.selectedSetup)
  Enabled(self.setupUndo, ready and self.hasSetupUndo)
  self.setupUpdate:SetText(self.setupConfirmation == "update:" .. tostring(self.selectedSetup) and "Confirm Update" or "Update selected")
  self.setupDelete:SetText(self.setupConfirmation == "delete:" .. tostring(self.selectedSetup) and "Confirm Delete" or "Delete selected")
end

function Z:PositionSetupWindow(talents)
  local window = self.setupWindow
  window:ClearAllPoints()
  if talents and TalentFrame then
    window:SetParent(TalentFrame)
    window:SetPoint("TOPLEFT", TalentFrame.backdrop or TalentFrame, "TOPRIGHT", 6, 0)
  else
    window:SetParent(UIParent)
    window:SetPoint("CENTER", UIParent, "CENTER", 200, 0)
  end
  self:UpdateTalentSetupButton()
end

function Z:ToggleSetups(talents)
  if not self.setupWindow then self:CreateSetupWindow() end
  if self.setupWindow:IsVisible() then self.setupWindow:Hide()
  else self:PositionSetupWindow(talents); self:RefreshSetups(); self.setupWindow:Show() end
end

function Z:UpdateTalentSetupButton()
  local button = self.talentSetupsButton
  if not button then return end
  if self.setupWindow and self.setupWindow:GetParent() == TalentFrame and self.setupWindow:IsShown() then
    button:LockHighlight()
  else button:UnlockHighlight() end
end

function Z:StyleTalentSetupButton()
  local button = self.talentSetupsButton
  if not button or not TalentFrame then return end
  button:ClearAllPoints()
  if TalentFrame.backdrop then
    -- pfUI's visible panel ends above the stock frame's unused texture area.
    button:SetPoint("BOTTOMLEFT", TalentFrame.backdrop, "BOTTOMLEFT", 12, 9)
    if not button.zProfilesPfSkinned and pfUI and pfUI.api and type(pfUI.api.SkinButton) == "function" then
      pfUI.api.SkinButton(button)
      button.zProfilesPfSkinned = true
    end
  else
    -- Match the stock Close button's footer row, to the left of talent points.
    button:SetPoint("BOTTOMLEFT", TalentFrame, "BOTTOMLEFT", 24, 81)
  end
  self:UpdateTalentSetupButton()
end

function Z:AttachTalentSetups()
  if not TalentFrame then return end
  if self.talentSetupsButton then self:StyleTalentSetupButton(); return end
  local button = CreateFrame("Button", "zProfilesTalentSetupsButton", TalentFrame, "UIPanelButtonTemplate")
  self.talentSetupsButton = button
  button.zProfilesPfSkinned = false
  button:SetWidth(112); button:SetHeight(22)
  local icon = button:CreateTexture(nil, "OVERLAY")
  icon:SetTexture("Interface\\Icons\\INV_Misc_Book_09")
  icon:SetWidth(14); icon:SetHeight(14)
  icon:SetPoint("LEFT", button, "LEFT", 7, 0)
  icon:SetTexCoord(0.08, 0.92, 0.08, 0.92)
  local text = button:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
  text:SetPoint("LEFT", button, "LEFT", 25, 0)
  text:SetPoint("RIGHT", button, "RIGHT", -5, 0)
  text:SetJustifyH("CENTER")
  button:SetFontString(text)
  button:SetNormalFontObject(GameFontNormalSmall)
  button:SetHighlightFontObject(GameFontHighlightSmall)
  button:SetText("Bar setups")
  button:SetScript("OnClick", function() Z:ToggleSetups(true) end)
  button:SetScript("OnEnter", function()
    if not GameTooltip then return end
    GameTooltip:SetOwner(this, "ANCHOR_RIGHT")
    GameTooltip:SetText("Bar setups")
    GameTooltip:AddLine("Save and swap this character's action bars and key bindings.", 1, 1, 1, true)
    GameTooltip:Show()
  end)
  button:SetScript("OnLeave", function() if GameTooltip then GameTooltip:Hide() end end)
  -- Respect the talent frame's original OnShow handler and skin hooks. Doing
  -- the layout afterward also handles skins that create their backdrop late.
  local onShow = TalentFrame:GetScript("OnShow")
  TalentFrame:SetScript("OnShow", function()
    if onShow then onShow() end
    Z:StyleTalentSetupButton()
  end)
  self:StyleTalentSetupButton()
end

local events = CreateFrame("Frame")
events:RegisterEvent("ADDON_LOADED")
events:RegisterEvent("PLAYER_ENTERING_WORLD")
events:RegisterEvent("PLAYER_REGEN_DISABLED")
events:RegisterEvent("PLAYER_REGEN_ENABLED")
events:SetScript("OnEvent", function()
  Z:AttachTalentSetups()
  if Z.setupWindow and Z.setupWindow:IsShown() then Z:RefreshSetups() end
end)
Z:AttachTalentSetups()
