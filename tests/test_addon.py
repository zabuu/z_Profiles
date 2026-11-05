"""Lua UI/controller tests using a mocked WoW API. No game files are read."""
from pathlib import Path
import unittest

from lupa.lua51 import LuaRuntime

ROOT = Path(__file__).resolve().parents[1]

BOOTSTRAP = r'''
frames, messages, calls = {}, {}, {}
reloads, barUpdates = 0, 0
UISpecialFrames, SlashCmdList = {}, {}
local methods = {}
function methods:SetText(text)
  self.text = text
  if rawget(self,'fontString') then self.fontString.text = text end
end
function methods:SetFontString(label) self.fontString = label end
function methods:SetFont(path,size,style) self.font = {path,size,style} end
function methods:GetStringWidth() return string.len(self.text or '') * ((rawget(self,'font') or {})[2] or 12) * .5 end
function methods:SetTextColor(...) self.color = {...} end
function methods:SetTexture(...) self.texture = {...} end
function methods:SetVertexColor(...) self.vertexColor = {...} end
function methods:SetBackdrop(backdrop) self.backdropInfo = backdrop end
function methods:SetBackdropColor(...) self.background = {...} end
function methods:SetBackdropBorderColor(...) self.borderColor = {...} end
function methods:SetAlpha(alpha) self.alpha = alpha end
function methods:GetAlpha() return rawget(self,'alpha') or 1 end
function methods:GetFrameLevel()
  if rawget(self,'level') then return self.level end
  local parent = rawget(self,'parent')
  return parent and parent:GetFrameLevel() + ((self.kind=='FontString' or self.kind=='Texture') and 0 or 1) or 1
end
function methods:SetFrameLevel(level) self.level = level end
function methods:GetWidth() return rawget(self,'width') end
function methods:GetHeight() return rawget(self,'height') end
function methods:GetRight() return rawget(self,'right') end
function methods:GetEffectiveScale() return rawget(self,'scale') or 1 end
function methods:SetFocus() self.focused = true end
function methods:ClearFocus() self.focused = false end
function methods:StartMoving() self.moving = true end
function methods:StopMovingOrSizing() self.moving = false end
function methods:SetAllPoints(target) self.allPoints = target end
function methods:SetJustifyH(value) self.justify = value end
function methods:SetWidth(width) self.width = width end
function methods:SetHeight(height) self.height = height end
function methods:SetScript(name, fn) self.scripts[name] = fn end
function methods:GetScript(name) return self.scripts[name] end
function methods:Show()
  local wasShown = self.shown
  self.shown = true
  if not wasShown and self.scripts.OnShow then
    local previousThis = this; this = self; self.scripts.OnShow(); this = previousThis
  end
end
function methods:Hide()
  local wasShown = self.shown
  self.shown = false
  if wasShown and self.scripts.OnHide then self.scripts.OnHide() end
end
function methods:IsShown() return self.shown end
function methods:IsVisible()
  local parent = rawget(self,'parent')
  return self.shown and (not parent or parent:IsVisible())
end
function methods:Enable() self.enabled = true end
function methods:Disable() self.enabled = false end
function methods:GetText() return self.text or '' end
function methods:SetParent(parent) self.parent = parent end
function methods:GetParent() return rawget(self,'parent') end
function methods:ClearAllPoints() self.points = {}; self.point = nil end
function methods:SetPoint(...)
  self.point = {...}
  if not rawget(self,'points') then self.points = {} end
  table.insert(self.points,self.point)
end
function methods:CreateFontString() return CreateFrame('FontString',nil,self) end
function methods:CreateTexture() return CreateFrame('Texture',nil,self) end
function methods:SetHighlightTexture(path)
  self.highlightTexture = self:CreateTexture(); self.highlightTexture:SetTexture(path)
end
function methods:GetHighlightTexture() return rawget(self,'highlightTexture') end
local metatable = {__index = function(t,k)
  if methods[k] then return methods[k] end
  if string.find(k,'^[A-Z]') then return function() end end
end}
function CreateFrame(kind,name,parent,template)
  local frame = setmetatable({kind=kind,name=name,parent=parent,scripts={},enabled=true,shown=true},metatable)
  table.insert(frames,frame)
  return frame
end
UIParent = CreateFrame('Frame','UIParent')
DEFAULT_CHAT_FRAME = {AddMessage=function(self,text) table.insert(messages,text) end}
function ReloadUI() reloads = reloads + 1 end
function MultiActionBar_Update() barUpdates = barUpdates + 1 end
function GetRealmName() return 'Realm' end
function UnitName() return 'Char1' end
function SetCVar() error('addon must not change CVars') end
function SetBinding() error('addon must not change bindings') end
function CreateMacro() error('addon must not rewrite macros') end
state = {active='default',desired='default',problem='ok',version=1,accountCount=12,charCount=8,online=true,fail=false,partial=false}
function Native(command,a,b,c)
  table.insert(calls,{command,a,b,c})
  if state.fail then error('simulated backend failure') end
  if command == 'version' then return state.version end
  if command == 'status' then return 'ACC01',state.active,state.desired,state.problem,10 end
  if command == 'accounts' then
    local result=''
    for i=1,state.accountCount do
      local name=string.format('ACC%02d',i)
      result=result..name..'\t'..(i==1 and state.active or 'default')..'\t'..(i==1 and state.desired or 'default')..'\n'
    end
    return result
  end
  if command == 'characters' then
    local result=''
    for i=1,state.charCount do result=result..'Realm\tChar'..i..'\t'..(i==1 and '1' or '0')..'\n' end
    return result
  end
  if command == 'setMode' then state.desired=b; return 1,'Mode saved. Fully exit and restart WoW to apply it.',0 end
  if command == 'revertAccount' or command == 'revertCharacter' then
    return state.partial and 0 or 1,'Reverted',state.online and 1 or 0
  end
  return 1,'ok',0
end
function Click(button)
  this=button
  button.scripts.OnClick()
  this=nil
end
function Fire(name)
  event=name
  -- The controller's first frame follows UIParent.
  frames[2].scripts.OnEvent()
  event=nil
end
'''


class AddonTests(unittest.TestCase):
    def setup_lua(self, native=True):
        lua = LuaRuntime(unpack_returned_tuples=True)
        lua.execute(BOOTSTRAP)
        if native:
            lua.execute('zProfilesNative=Native; zAPI=function(command) if command == "profilesVersion" then return 1 end end')
        for name in ('Core.lua', 'Options.lua', 'Pie.lua', 'Setups.lua', 'SetupUI.lua'):
            lua.execute((ROOT / name).read_text(encoding='utf-8'))
        return lua

    def test_missing_dll_disables_profile_controls(self):
        lua = self.setup_lua(False)
        lua.execute('SlashCmdList.ZPROFILES()')
        self.assertFalse(lua.eval('zProfiles.isolateButton.enabled'))
        self.assertFalse(lua.eval('zProfiles.revertAccountButton.enabled'))
        self.assertIn('not loaded', lua.eval('zProfiles.statusLabel.text'))
        self.assertEqual(lua.eval('table.getn(calls)'), 0)

    def test_old_standalone_bridge_does_not_enable_new_addon(self):
        lua = self.setup_lua()
        lua.execute('zAPI=function() return nil end; SlashCmdList.ZPROFILES()')
        self.assertFalse(lua.eval('zProfiles.isolateButton.enabled'))
        self.assertFalse(lua.eval('zProfiles.revertAccountButton.enabled'))
        self.assertIn('zAPI.dll', lua.eval('zProfiles.noticeLabel.text'))

    def test_failed_merged_capability_probe_disables_controls(self):
        lua = self.setup_lua()
        lua.execute('zAPI=function() error("unavailable") end; SlashCmdList.ZPROFILES()')
        self.assertFalse(lua.eval('zProfiles.isolateButton.enabled'))

    def test_default_account_only_changes_mode_on_request(self):
        lua = self.setup_lua()
        lua.execute('zProfiles:Toggle(); Fire("PLAYER_LOGIN")')
        self.assertEqual(lua.eval('barUpdates'), 0)
        self.assertEqual(lua.eval('reloads'), 0)
        self.assertFalse(lua.eval('zProfiles.revertAccountButton.enabled'))
        lua.execute('Click(zProfiles.isolateButton)')
        self.assertEqual(lua.eval('state.desired'), 'isolated')
        self.assertEqual(lua.eval('state.active'), 'default')
        self.assertIn('After restart: isolated', lua.eval('zProfiles.accountModeLabel.text'))
        self.assertEqual(lua.eval('reloads'), 0)

    def test_account_and_character_pagination(self):
        lua = self.setup_lua()
        lua.execute('zProfiles:Toggle(); Click(zProfiles.nextAccounts)')
        self.assertEqual(lua.eval('zProfiles.accountButtons[1].account'), 'ACC10')
        lua.execute('Click(zProfiles.accountButtons[1]); Click(zProfiles.nextCharacters)')
        self.assertEqual(lua.eval('zProfiles.selected'), 'ACC10')
        self.assertEqual(lua.eval('zProfiles.revertButtons[1].character'), 'Char6')
        self.assertFalse(lua.eval('zProfiles.revertButtons[4].shown'))

    def test_character_revert_has_exact_scope_and_reloads_current(self):
        lua = self.setup_lua()
        lua.execute('state.active="isolated"; state.desired="isolated"; zProfiles:Toggle(); Click(zProfiles.revertButtons[1])')
        self.assertEqual(lua.eval('reloads'), 1)
        self.assertEqual(lua.eval('calls[table.getn(calls)][1]'), 'revertCharacter')
        self.assertEqual(lua.eval('calls[table.getn(calls)][2]'), 'ACC01')
        self.assertEqual(lua.eval('calls[table.getn(calls)][3]'), 'Realm')
        self.assertEqual(lua.eval('calls[table.getn(calls)][4]'), 'Char1')

    def test_offline_revert_does_not_reload_current_character(self):
        lua = self.setup_lua()
        lua.execute('state.active="isolated"; state.desired="isolated"; state.online=false; zProfiles:Toggle(); Click(zProfiles.revertButtons[2])')
        self.assertEqual(lua.eval('reloads'), 0)

    def test_account_revert_uses_separate_native_command(self):
        lua = self.setup_lua()
        lua.execute('state.active="isolated"; state.desired="isolated"; zProfiles:Toggle(); Click(zProfiles.revertAccountButton)')
        self.assertEqual(lua.eval('calls[table.getn(calls)][1]'), 'revertAccount')
        self.assertEqual(lua.eval('reloads'), 1)

    def test_partial_current_revert_still_reloads_stale_ui(self):
        lua = self.setup_lua()
        lua.execute('state.active="isolated"; state.partial=true; zProfiles:Toggle(); zProfiles:RevertCharacter("Realm","Char1")')
        self.assertEqual(lua.eval('reloads'), 1)

    def test_events_identify_character_and_only_refresh_isolated_bars(self):
        lua = self.setup_lua()
        lua.execute('state.active="isolated"; state.desired="isolated"; Fire("PLAYER_LOGIN")')
        self.assertEqual(lua.eval('calls[2][1]'), 'identify')
        self.assertEqual(lua.eval('calls[2][2]'), 'Realm')
        self.assertEqual(lua.eval('barUpdates'), 1)
        self.assertIn('Isolation active', lua.eval('messages[1]'))

    def test_backend_error_does_not_break_window(self):
        lua = self.setup_lua()
        lua.execute('state.fail=true; zProfiles:Toggle()')
        self.assertFalse(lua.eval('zProfiles.isolateButton.enabled'))
        self.assertTrue(lua.eval('zProfiles.window.shown'))

    def test_incompatible_contract_cannot_enable_isolation(self):
        lua = self.setup_lua()
        lua.execute('state.version=2; zProfiles:Toggle()')
        self.assertFalse(lua.eval('zProfiles.available'))
        self.assertFalse(lua.eval('zProfiles.isolateButton.enabled'))

    def test_fault_keeps_revert_available(self):
        lua = self.setup_lua()
        lua.execute('state.active="isolated"; state.desired="isolated"; state.problem="Checkpoint failed"; zProfiles:Toggle()')
        self.assertTrue(lua.eval('zProfiles.revertAccountButton.enabled'))
        self.assertIn('Checkpoint failed', lua.eval('zProfiles.statusLabel.text'))

    def test_uses_lua_50_compatible_constructs(self):
        for name in ('Core.lua', 'Options.lua', 'Pie.lua', 'Setups.lua', 'SetupUI.lua'):
            text = (ROOT / name).read_text(encoding='utf-8')
            self.assertNotIn('string.match', text)
            self.assertNotIn('string.gmatch', text)
            self.assertNotIn('table.unpack', text)
            self.assertNotIn('#', text)


if __name__ == '__main__':
    unittest.main(verbosity=2)
