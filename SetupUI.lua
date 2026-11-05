local Z = zProfiles
local WIDTH, PAD, ROWS = 168, 12, 5
local WHITE = "Interface\\Buttons\\WHITE8X8"
local function Font() return pfUI and pfUI.font_default or "Fonts\\ARIALN.TTF" end
local function Label(parent, text, size, x, y, width)
  local label = parent:CreateFontString(nil, "OVERLAY")
  label:SetFont(Font(), size)
  label:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y)
  label:SetJustifyH("LEFT"); label:SetJustifyV("TOP")
  label:SetTextColor(0.9, 0.92, 0.96)
  if width then label:SetWidth(width) end
  label:SetText(text)
  return label
end
local function Surface(frame, r, g, b)
  frame:SetBackdrop({ bgFile = WHITE, edgeFile = WHITE, edgeSize = 1,
    insets = { left = 1, right = 1, top = 1, bottom = 1 } })
  frame:SetBackdropColor(r, g, b, 0.98)
  frame:SetBackdropBorderColor(0.26, 0.28, 0.33, 1)
end
local function Tip(frame, title, description)
  frame:SetScript("OnEnter", function()
    if not GameTooltip then return end
    GameTooltip:SetOwner(this, "ANCHOR_RIGHT")
    GameTooltip:SetText(type(title) == "function" and title() or title)
    if description then GameTooltip:AddLine(type(description) == "function" and description() or description, 0.8, 0.83, 0.9, true) end
    GameTooltip:Show()
  end)
  frame:SetScript("OnLeave", function() if GameTooltip then GameTooltip:Hide() end end)
end
local function Button(parent, text, width, height, x, y, click, primary)
  local button = CreateFrame("Button", nil, parent)
  button:SetWidth(width); button:SetHeight(height)
  button:SetPoint("TOPLEFT", parent, "TOPLEFT", x, y)
  button.fill = button:CreateTexture(nil, "BACKGROUND")
  button.fill:SetAllPoints(button)
  if primary then button.fill:SetTexture(0.28, 0.36, 0.56, 1)
  else button.fill:SetTexture(0.12, 0.14, 0.18, 0) end
  button.label = Label(button, text, 12, 0, 0, width)
  button.label:ClearAllPoints(); button.label:SetPoint("CENTER", button, "CENTER", 0, 0)
  button.label:SetJustifyH("CENTER"); button.label:SetHeight(15)
  button:SetFontString(button.label); button:SetText(text)
  button:SetHighlightTexture(WHITE)
  local highlight = button:GetHighlightTexture()
  if highlight then highlight:SetVertexColor(0.7, 0.78, 1, 0.12) end
  button:SetScript("OnClick", click)
  return button
end
local function Enabled(button, enabled)
  if enabled then button:Enable(); button:SetAlpha(1)
  else button:Disable(); button:SetAlpha(0.35) end
end
local function Visible(frame, shown) if shown then frame:Show() else frame:Hide() end end
local function Place(frame, parent, y)
  frame:ClearAllPoints(); frame:SetPoint("TOPLEFT", parent, "TOPLEFT", PAD, y)
end
local function Fit(label, name, width)
  local text = name
  label:SetWidth(0); label:SetText(text)
  while label:GetStringWidth() > width and string.len(text) > 1 do
    text = string.sub(text, 1, -2)
    label:SetText(text .. "...")
  end
  text = text == name and name or text .. "..."
  label:SetText(text); label:SetWidth(width)
  return text
end

function Z:SelectSetup(name)
  self.selectedSetup = name
  self.setupConfirmation, self.setupNotice = nil, nil
  self.setupMenu:Hide()
  self:RenderSetups()
end
function Z:PageSetups(delta)
  local names = self.setupNames or {}
  local pages = math.max(1, math.ceil(table.getn(names) / ROWS))
  self.setupPage = math.max(1, math.min(pages, (self.setupPage or 1) + delta))
  self:SelectSetup(names[(self.setupPage - 1) * ROWS + 1])
end
function Z:ToggleSetupMenu()
  self.setupConfirmation, self.setupNotice = nil, nil
  Visible(self.setupMenu, not self.setupMenu:IsShown())
  self:RenderSetups()
end
function Z:OpenSetupEditor(mode)
  if mode == "rename" and not self.selectedSetup then return end
  self.setupMenu:Hide()
  self.setupEditorMode, self.setupConfirmation, self.setupNotice = mode, nil, nil
  self.setupNameInput:SetText(mode == "rename" and self.selectedSetup or "")
  self:RenderSetups()
  self.setupNameInput:SetFocus()
  if mode == "rename" then self.setupNameInput:HighlightText() end
end
function Z:CloseSetupEditor()
  self.setupEditorMode = nil
  self.setupNameInput:ClearFocus()
  self:RenderSetups()
end
function Z:SubmitSetupEditor()
  local mode = self.setupEditorMode
  if not mode then return end
  local name = string.gsub(self.setupNameInput:GetText(), "^%s*(.-)%s*$", "%1")
  local saved
  if mode == "rename" then saved = self:RenameSetup(name)
  else saved = self:SaveSetup(name) end
  if saved then
    for i, entry in ipairs(self.setupNames) do
      if entry == name then self.setupPage = math.floor((i - 1) / ROWS) + 1 end
    end
    self:CloseSetupEditor()
  end
end
function Z:ConfirmSetupAction(kind)
  if not self.selectedSetup then return end
  if self.setupConfirmation == kind .. ":" .. self.selectedSetup then
    self.setupConfirmation = nil
    self.setupMenu:Hide()
    if kind == "update" then self:SaveSetup(self.selectedSetup, true) else self:DeleteSetup() end
  else
    self.setupConfirmation = kind .. ":" .. self.selectedSetup
    self:SetupNotice(kind == "update" and "Replace this saved setup?\nClick Confirm update." or "Delete this saved setup?\nClick Confirm delete.")
  end
end

function Z:CreateSetupWindow()
  local window = CreateFrame("Frame", "zProfilesSetupWindow", UIParent)
  self.setupWindow = window
  window:SetWidth(WIDTH); window:SetHeight(416)
  window:SetFrameStrata("DIALOG"); window:EnableMouse(true)
  window:SetMovable(true); window:SetClampedToScreen(true)
  window:RegisterForDrag("LeftButton")
  window:SetScript("OnDragStart", function()
    if window:GetParent() == UIParent then window:StartMoving() end
  end)
  window:SetScript("OnDragStop", function() window:StopMovingOrSizing() end)
  window:EnableMouseWheel(true)
  window:SetScript("OnMouseWheel", function()
    if not Z.setupEditorMode then Z:PageSetups(arg1 > 0 and -1 or 1) end
  end)
  window:SetScript("OnMouseDown", function() Z.setupMenu:Hide(); Z.setupConfirmation = nil; Z:RenderSetups() end)
  Surface(window, 0.075, 0.085, 0.105)
  Label(window, "Setups", 14, PAD, -15, 66)
  self.setupNew = Button(window, "+", 22, 24, 86, -10, function() Z:OpenSetupEditor("new") end)
  self.setupMore = Button(window, "...", 22, 24, 110, -10, function() Z:ToggleSetupMenu() end)
  self.setupClose = Button(window, "x", 22, 24, 134, -10, function() window:Hide() end)
  Tip(self.setupNew, "Save current setup", "Capture your current bars, bindings and zPie ring slots.")
  Tip(self.setupMore, "Setup actions", "Update, rename or delete the selected setup.")
  Tip(self.setupClose, "Hide setups")
  self.setupCharacterLabel = Label(window, "", 11, PAD, -40, WIDTH - PAD * 2)
  self.setupCharacterLabel:SetTextColor(0.56, 0.61, 0.7)
  self.setupCharacterLabel:SetHeight(14)
  local line = window:CreateTexture(nil, "BORDER")
  line:SetTexture(0.25, 0.28, 0.34, 0.55)
  line:SetPoint("TOPLEFT", window, "TOPLEFT", PAD, -61)
  line:SetWidth(WIDTH - PAD * 2); line:SetHeight(1)
  self.setupRows = {}
  for i = 1, ROWS do
    local row = Button(window, "", WIDTH - PAD * 2, 30, PAD, -70 - (i - 1) * 32,
      function() Z:SelectSetup(this.setupName) end)
    self.setupRows[i] = row
    row.label:ClearAllPoints(); row.label:SetPoint("LEFT", row, "LEFT", 10, 0)
    row.label:SetJustifyH("LEFT")
    row.mark = row:CreateTexture(nil, "OVERLAY")
    row.mark:SetTexture(0.64, 0.72, 1, 1)
    row.mark:SetPoint("LEFT", row, "LEFT", 0, 0); row.mark:SetWidth(2); row.mark:SetHeight(18)
    row.last = row:CreateTexture(nil, "OVERLAY")
    row.last:SetTexture(0.64, 0.72, 1, 1)
    row.last:SetPoint("RIGHT", row, "RIGHT", -7, 0); row.last:SetWidth(4); row.last:SetHeight(4)
    Tip(row, function() return row.setupName or "Setup" end, function()
      return row.setupName == Z.lastAppliedSetup and "Last applied setup. Select, then Apply to load it again." or "Select this setup, then choose Apply."
    end)
  end
  self.setupEmpty = Label(window, "No saved setups\n\nSave your current bars and rings to get started.", 12, PAD + 4, -85, WIDTH - PAD * 2 - 8)
  self.setupEmpty:SetHeight(110); self.setupEmpty:SetTextColor(0.64, 0.69, 0.77)
  self.setupPrevious = Button(window, "<", 24, 22, PAD, -234, function() Z:PageSetups(-1) end)
  self.setupPageLabel = Label(window, "", 11, 48, -239, 72)
  self.setupPageLabel:SetJustifyH("CENTER"); self.setupPageLabel:SetTextColor(0.56, 0.61, 0.7)
  self.setupNext = Button(window, ">", 24, 22, WIDTH - PAD - 24, -234, function() Z:PageSetups(1) end)
  self.setupApply = Button(window, "Apply", WIDTH - PAD * 2, 28, PAD, -268, function()
    Z.setupMenu:Hide(); Z.setupConfirmation = nil
    if Z.selectedSetup then Z:ApplySetup(Z.selectedSetup) else Z:OpenSetupEditor("new") end
  end, true)
  Tip(self.setupApply, function() return Z.selectedSetup and "Apply " .. Z.selectedSetup or "Save first setup" end,
    "Load the selected bars, bindings and saved ring slots. Change talents first.")
  self.setupUndo = Button(window, "Undo last switch", WIDTH - PAD * 2, 24, PAD, -302,
    function() Z.setupMenu:Hide(); Z.setupConfirmation = nil; Z:ApplySetup(nil, true) end)
  Tip(self.setupUndo, "Undo last switch", "Restore the unsaved setup captured before the last Apply.")
  self.setupLastLabel = Label(window, "", 11, PAD, -340, WIDTH - PAD * 2)
  self.setupLastLabel:SetHeight(14); self.setupLastLabel:SetTextColor(0.56, 0.61, 0.7)
  self.setupLastLabel:ClearAllPoints(); self.setupLastLabel:SetPoint("BOTTOMLEFT", window, "BOTTOMLEFT", PAD, 78)
  self.setupStatusArea = CreateFrame("Frame", nil, window)
  self.setupStatusArea:SetPoint("BOTTOMLEFT", window, "BOTTOMLEFT", PAD, 14)
  self.setupStatusArea:SetWidth(WIDTH - PAD * 2); self.setupStatusArea:SetHeight(58)
  self.setupStatusArea:EnableMouse(true)
  self.setupStatusLabel = Label(self.setupStatusArea, "", 11, 0, 0, WIDTH - PAD * 2)
  self.setupStatusLabel:SetHeight(58)
  Tip(self.setupStatusArea, "Setup status", function()
    return (Z.lastAppliedSetup and "Last applied: " .. Z.lastAppliedSetup .. "\n\n" or "") ..
      (Z.setupsReason or Z.setupNotice or "Setups save bars, bindings and optional zPie ring slots. Talent switching is manual. The small dot marks the last applied setup.")
  end)

  local editor = CreateFrame("Frame", nil, window)
  self.setupEditor = editor
  editor:SetPoint("TOPLEFT", window, "TOPLEFT", PAD, -75)
  editor:SetWidth(WIDTH - PAD * 2); editor:SetHeight(194)
  Label(editor, "Setup name", 12, 0, 0, WIDTH - PAD * 2)
  self.setupEditorHint = Label(editor, "", 11, 0, -80, WIDTH - PAD * 2)
  self.setupEditorHint:SetTextColor(0.56, 0.61, 0.7); self.setupEditorHint:SetHeight(48)
  local edit = CreateFrame("EditBox", "zProfilesSetupName", editor)
  self.setupNameInput = edit
  edit:SetWidth(WIDTH - PAD * 2); edit:SetHeight(28)
  edit:SetPoint("TOPLEFT", editor, "TOPLEFT", 0, -26)
  edit:SetFont(Font(), 12); edit:SetTextInsets(8, 8, 0, 0)
  edit:SetAutoFocus(false); edit:SetMaxLetters(32)
  Surface(edit, 0.045, 0.055, 0.07)
  edit:SetScript("OnEscapePressed", function() Z:CloseSetupEditor() end)
  edit:SetScript("OnEnterPressed", function() Z:SubmitSetupEditor() end)
  self.setupSave = Button(editor, "Save setup", WIDTH - PAD * 2, 28, 0, -136, function() Z:SubmitSetupEditor() end, true)
  self.setupCancel = Button(editor, "Cancel", WIDTH - PAD * 2, 24, 0, -170, function() Z:CloseSetupEditor() end)
  editor:Hide()

  local menu = CreateFrame("Frame", nil, window)
  self.setupMenu = menu
  menu:SetWidth(WIDTH - 16); menu:SetHeight(106)
  menu:SetPoint("TOPRIGHT", window, "TOPRIGHT", -8, -38)
  menu:SetFrameLevel(window:GetFrameLevel() + 10); menu:EnableMouse(true)
  Surface(menu, 0.12, 0.135, 0.165)
  self.setupUpdate = Button(menu, "Update saved setup", WIDTH - 28, 30, 6, -6, function() Z:ConfirmSetupAction("update") end)
  self.setupRename = Button(menu, "Rename...", WIDTH - 28, 30, 6, -38, function() Z:OpenSetupEditor("rename") end)
  self.setupDelete = Button(menu, "Delete...", WIDTH - 28, 30, 6, -70, function() Z:ConfirmSetupAction("delete") end)
  self.setupDelete.label:SetTextColor(1, 0.56, 0.53)
  menu:Hide()
  window:SetScript("OnShow", function()
    Z.setupNotice, Z.setupConfirmation = nil, nil
    Z:RefreshSetups(); Z:UpdateTalentSetupButton()
    window.fadeElapsed = 0; window:SetAlpha(0)
    window:SetScript("OnUpdate", function()
      window.fadeElapsed = window.fadeElapsed + arg1
      window:SetAlpha(math.min(1, window.fadeElapsed / 0.12))
      if window.fadeElapsed >= 0.12 then window:SetScript("OnUpdate", nil) end
    end)
  end)
  window:SetScript("OnHide", function()
    menu:Hide(); editor:Hide(); edit:ClearFocus()
    Z.setupEditorMode, Z.setupConfirmation = nil, nil
    window:SetScript("OnUpdate", nil); window:SetAlpha(1)
    Z:UpdateTalentSetupButton()
  end)
  if UISpecialFrames then table.insert(UISpecialFrames, "zProfilesSetupWindow") end
  window:Hide()
end

function Z:RenderSetups()
  if not self.setupWindow then return end
  local names, editing = self.setupNames or {}, self.setupEditorMode ~= nil
  local count = table.getn(names)
  local pages = math.max(1, math.ceil(count / ROWS))
  self.setupPage = math.min(self.setupPage or 1, pages)
  self.setupCharacterLabel:SetText(editing and (self.setupEditorMode == "rename" and "Rename setup" or "New setup") or tostring(UnitName("player")))
  Fit(self.setupLastLabel, self.lastAppliedSetup and "Last applied: " .. self.lastAppliedSetup or "Select a setup to apply.", WIDTH - PAD * 2)
  Visible(self.setupLastLabel, not editing)
  local status = self.setupsReason or self.setupNotice or "Bars / Keys / Rings\nChange talents, then apply."
  if editing and not self.setupsReason and not self.setupNotice then status = "" end
  if string.len(status) > 112 then status = string.sub(status, 1, 109) .. "..." end
  self.setupStatusLabel:SetText(status)
  if self.setupsReason or self.setupConfirmation then self.setupStatusLabel:SetTextColor(1, 0.75, 0.45)
  else self.setupStatusLabel:SetTextColor(0.64, 0.69, 0.77) end
  local ready = self.setupsReady
  for i, row in ipairs(self.setupRows) do
    local name = names[(self.setupPage - 1) * ROWS + i]
    row.setupName = name
    Visible(row, name and not editing)
    if name then
      row:SetText(Fit(row.label, name, WIDTH - PAD * 2 - 28))
      local selected = self.selectedSetup == name
      row.fill:SetTexture(0.23, 0.29, 0.43, selected and 0.75 or 0)
      Visible(row.mark, selected); Visible(row.last, name == self.lastAppliedSetup)
    end
  end
  Visible(self.setupEmpty, count == 0 and not editing and ready)
  local paging = pages > 1 and not editing
  Visible(self.setupPrevious, paging); Visible(self.setupNext, paging); Visible(self.setupPageLabel, paging)
  self.setupPageLabel:SetText(self.setupPage .. " / " .. pages)
  Enabled(self.setupPrevious, self.setupPage > 1); Enabled(self.setupNext, self.setupPage < pages)
  local shownRows = math.min(ROWS, math.max(1, count - (self.setupPage - 1) * ROWS))
  local applyY = -(70 + shownRows * 32 + (paging and 30 or 12))
  if count == 0 then applyY = -210 end
  Place(self.setupApply, self.setupWindow, applyY)
  Place(self.setupUndo, self.setupWindow, applyY - 34)
  Visible(self.setupApply, not editing); Visible(self.setupUndo, not editing and (count > 0 or self.hasSetupUndo))
  self.setupApply:SetText(ready and count == 0 and "Save setup" or "Apply")
  Enabled(self.setupApply, ready and (count == 0 or self.selectedSetup))
  Enabled(self.setupNew, ready and not editing)
  Enabled(self.setupMore, ready and self.selectedSetup and not editing)
  Enabled(self.setupSave, ready)
  Enabled(self.setupUpdate, ready and self.selectedSetup)
  Enabled(self.setupDelete, ready and self.selectedSetup)
  Enabled(self.setupRename, ready and self.selectedSetup)
  Enabled(self.setupUndo, ready and self.hasSetupUndo)
  self.setupUpdate:SetText(self.setupConfirmation == "update:" .. tostring(self.selectedSetup) and "Confirm update" or "Update saved setup")
  self.setupDelete:SetText(self.setupConfirmation == "delete:" .. tostring(self.selectedSetup) and "Confirm delete" or "Delete...")
  self.setupSave:SetText(self.setupEditorMode == "rename" and "Rename setup" or "Save setup")
  self.setupEditorHint:SetText(self.setupEditorMode == "rename" and "Give this saved setup a new name." or "Save your current bars, bindings and ring assignments.")
  Visible(self.setupEditor, editing)
end

function Z:PositionSetupWindow(talents)
  local window = self.setupWindow
  window:ClearAllPoints()
  if talents and TalentFrame then
    window:SetParent(TalentFrame)
    local surface = TalentFrame.backdrop or TalentFrame
    local right, width = surface:GetRight(), UIParent:GetWidth()
    local scale, uiScale = surface:GetEffectiveScale(), UIParent:GetEffectiveScale()
    local left = type(right) == "number" and type(width) == "number" and type(scale) == "number" and
      type(uiScale) == "number" and uiScale > 0 and right * scale / uiScale + WIDTH > width - 8
    self.setupDockSide = left and "left" or "right"
    if TalentFrame.backdrop then
      window:SetPoint(left and "TOPRIGHT" or "TOPLEFT", surface, left and "TOPLEFT" or "TOPRIGHT", left and -1 or 1, 0)
      window:SetPoint(left and "BOTTOMRIGHT" or "BOTTOMLEFT", surface, left and "BOTTOMLEFT" or "BOTTOMRIGHT", left and -1 or 1, 0)
    else
      window:SetPoint(left and "TOPRIGHT" or "TOPLEFT", surface, left and "TOPLEFT" or "TOPRIGHT", left and 15 or -32, -10)
      window:SetPoint(left and "BOTTOMRIGHT" or "BOTTOMLEFT", surface, left and "BOTTOMLEFT" or "BOTTOMRIGHT", left and 15 or -32, 72)
    end
  else
    self.setupDockSide = nil
    window:SetParent(UIParent); window:SetHeight(416)
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
  button:SetWidth(92); button:SetHeight(22)
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
  button:SetText("Setups")
  button:SetScript("OnClick", function() Z:ToggleSetups(true) end)
  button:SetScript("OnEnter", function()
    if not GameTooltip then return end
    GameTooltip:SetOwner(this, "ANCHOR_RIGHT")
    GameTooltip:SetText("Character setups")
    GameTooltip:AddLine("Save and swap bars, key bindings and zPie ring assignments.", 1, 1, 1, true)
    GameTooltip:Show()
  end)
  button:SetScript("OnLeave", function() if GameTooltip then GameTooltip:Hide() end end)
  -- Respect the talent frame's original OnShow handler and skin hooks. Doing
  -- the layout afterward also handles skins that create their backdrop late.
  local onShow = TalentFrame:GetScript("OnShow")
  TalentFrame:SetScript("OnShow", function()
    if onShow then onShow() end
    Z:StyleTalentSetupButton()
    if Z.setupWindow and Z.setupWindow:GetParent() == TalentFrame and Z.setupWindow:IsShown() then Z:PositionSetupWindow(true) end
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
