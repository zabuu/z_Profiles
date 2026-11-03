"""Binding swaps, native failure recovery, and talent-panel lifecycle."""
from pathlib import Path
import unittest

from lupa.lua51 import LuaRuntime
from test_addon import BOOTSTRAP

ROOT = Path(__file__).resolve().parents[1]
MOCK = r'''
state.active, state.desired = 'isolated', 'isolated'
state.setupVersion, state.combat, state.cursor = 1, false, false
setups, currentBars, undoSetup, lastSavedSet = {}, 'Frost bars', nil, 1
bindingCommands = {'HEADER_TEST','MOVEFORWARD','ACTIONBUTTON1','SPELL Frostbolt'}
bindings = {W='MOVEFORWARD', ['1']='ACTIONBUTTON1', ['SHIFT-1']='ACTIONBUTTON1', ['CTRL-F']='SPELL Frostbolt'}
function GetNumBindings() return table.getn(bindingCommands) end
function GetBindingKey(command)
  local result = {}
  for key, value in pairs(bindings) do if value == command then table.insert(result,key) end end
  table.sort(result)
  return unpack(result)
end
function GetBinding(index)
  local command = bindingCommands[index]
  return command, GetBindingKey(command)
end
function GetCurrentBindingSet() return lastSavedSet end
function SetBinding(key,command)
  if state.badKey == key and command == 'MOVEFORWARD' then return nil end
  bindings[key] = command
  return 1
end
function SaveBindings(which)
  if state.saveFailure then state.saveFailure = false; error('disk failure') end
  lastSavedSet = which
end
function UnitAffectingCombat() return state.combat end
function CursorHasItem() return state.cursor end
function CursorHasSpell() return false end
local originalNative = Native
function Native(command,a,b,c)
  if command == 'setupVersion' then return state.setupVersion end
  if command == 'setups' then
    local names = {}
    for name in pairs(setups) do table.insert(names,name) end
    table.sort(names)
    return table.concat(names,'\n'), undoSetup and 1 or 0
  end
  if command == 'saveSetup' or command == 'updateSetup' or command == 'saveSetupUndo' then
    local value = {bindings=b,bars=currentBars}
    if command == 'saveSetupUndo' then undoSetup=value
    else
      if command == 'saveSetup' and setups[a] then return 0,'Name exists' end
      setups[a] = value
    end
    return 1,'Saved '..a
  end
  if command == 'setupBindings' or command == 'setupUndoBindings' then
    local value = command == 'setupUndoBindings' and undoSetup or setups[a]
    if not value then return 0,'Missing setup' end
    return 1,value.bindings
  end
  if command == 'applySetup' or command == 'applySetupUndo' then
    if state.applyFailure then return 0,'Native apply failed' end
    local value = command == 'applySetupUndo' and undoSetup or setups[a]
    assert(value.bindings == b)
    currentBars = value.bars
    return 1,'Applied '..a
  end
  if command == 'deleteSetup' then setups[a]=nil; return 1,'Deleted' end
  if command == 'renameSetup' then setups[b]=setups[a]; setups[a]=nil; return 1,'Renamed' end
  return originalNative(command,a,b,c)
end
zProfilesNative = Native
'''


class SetupTests(unittest.TestCase):
    def lua(self):
        lua = LuaRuntime(unpack_returned_tuples=True)
        lua.execute(BOOTSTRAP + MOCK)
        for name in ('Core.lua', 'Options.lua', 'Setups.lua', 'SetupUI.lua'):
            lua.execute((ROOT / name).read_text(encoding='utf-8'))
        return lua

    def test_save_and_apply_preserves_both_keys_and_clears_extra_keys(self):
        lua = self.lua()
        lua.execute('''
          zProfiles:SaveSetup('Frost')
          currentBars='Fire bars'; bindings={W='ACTIONBUTTON1', ['1']='MOVEFORWARD', Q='SPELL Frostbolt'}
          zProfiles:ApplySetup('Frost')
        ''')
        self.assertEqual(lua.eval('currentBars'), 'Frost bars')
        self.assertEqual(lua.eval('bindings.W'), 'MOVEFORWARD')
        self.assertEqual(lua.eval('bindings["1"]'), 'ACTIONBUTTON1')
        self.assertEqual(lua.eval('bindings["SHIFT-1"]'), 'ACTIONBUTTON1')
        self.assertEqual(lua.eval('bindings["CTRL-F"]'), 'SPELL Frostbolt')
        self.assertIsNone(lua.eval('bindings.Q'))
        self.assertEqual(lua.eval('lastSavedSet'), 2)
        self.assertEqual(lua.eval('reloads'), 0)

    def test_undo_restores_unsaved_setup_before_the_switch(self):
        lua = self.lua()
        lua.execute('''
          zProfiles:SaveSetup('Frost')
          currentBars='Unsaved fire bars'; bindings={Q='MOVEFORWARD'}
          zProfiles:ApplySetup('Frost'); zProfiles:ApplySetup(nil,true)
        ''')
        self.assertEqual(lua.eval('currentBars'), 'Unsaved fire bars')
        self.assertEqual(lua.eval('bindings.Q'), 'MOVEFORWARD')
        self.assertIsNone(lua.eval('bindings.W'))

    def test_failed_native_apply_restores_bindings_and_their_original_scope(self):
        lua = self.lua()
        lua.execute('''
          zProfiles:SaveSetup('Frost'); bindings={Q='MOVEFORWARD'}; currentBars='Fire bars'
          state.applyFailure=true; zProfiles:ApplySetup('Frost')
        ''')
        self.assertEqual(lua.eval('bindings.Q'), 'MOVEFORWARD')
        self.assertEqual(lua.eval('currentBars'), 'Fire bars')
        self.assertEqual(lua.eval('lastSavedSet'), 1)
        self.assertIn('Previous bindings restored', lua.eval('zProfiles.setupNotice'))

    def test_binding_failure_never_changes_bars(self):
        lua = self.lua()
        lua.execute('''
          zProfiles:SaveSetup('Frost'); bindings={Q='ACTIONBUTTON1'}; currentBars='Fire bars'
          state.badKey='W'; zProfiles:ApplySetup('Frost')
        ''')
        self.assertEqual(lua.eval('currentBars'), 'Fire bars')
        self.assertEqual(lua.eval('bindings.Q'), 'ACTIONBUTTON1')
        self.assertEqual(lua.eval('lastSavedSet'), 1)

    def test_binding_save_failure_restores_bindings_without_applying_bars(self):
        lua = self.lua()
        lua.execute('''
          zProfiles:SaveSetup('Frost'); bindings={Q='MOVEFORWARD'}; currentBars='Fire bars'
          state.saveFailure=true; zProfiles:ApplySetup('Frost')
        ''')
        self.assertEqual(lua.eval('currentBars'), 'Fire bars')
        self.assertEqual(lua.eval('bindings.Q'), 'MOVEFORWARD')

    def test_default_combat_cursor_old_backend_and_fault_block_mutations(self):
        for expression in ('state.active="default"', 'state.combat=true', 'state.cursor=true',
                           'state.setupVersion=0', 'state.problem="checkpoint failed"'):
            with self.subTest(expression=expression):
                lua = self.lua()
                lua.execute(expression + '; zProfiles:SaveSetup("Blocked"); zProfiles:ToggleSetups()')
                self.assertIsNone(lua.eval('setups.Blocked'))
                self.assertFalse(lua.eval('zProfiles.setupSave.enabled'))
                self.assertFalse(lua.eval('zProfiles.setupUndo.enabled'))

    def test_corrupt_binding_data_is_rejected_before_any_changes(self):
        lua = self.lua()
        lua.execute("setups.Bad={bindings='ZPB1\\n00\\t41\\n',bars='bad'}; zProfiles:ApplySetup('Bad')")
        self.assertEqual(lua.eval('currentBars'), 'Frost bars')
        self.assertEqual(lua.eval('bindings.W'), 'MOVEFORWARD')
        self.assertIsNone(lua.eval('undoSetup'))

    def test_blank_binding_set_can_be_saved_and_applied(self):
        lua = self.lua()
        lua.execute("bindings={}; zProfiles:SaveSetup('Empty'); bindings={Q='MOVEFORWARD'}; zProfiles:ApplySetup('Empty')")
        self.assertIsNone(lua.eval('next(bindings)'))

    def test_overwrite_and_delete_require_second_click(self):
        lua = self.lua()
        lua.execute("zProfiles:SaveSetup('Frost'); zProfiles:ToggleSetups(); currentBars='changed'; Click(zProfiles.setupUpdate)")
        self.assertEqual(lua.eval('setups.Frost.bars'), 'Frost bars')
        lua.execute('Click(zProfiles.setupUpdate)')
        self.assertEqual(lua.eval('setups.Frost.bars'), 'changed')
        lua.execute('Click(zProfiles.setupDelete)')
        self.assertIsNotNone(lua.eval('setups.Frost'))
        lua.execute('Click(zProfiles.setupDelete)')
        self.assertIsNone(lua.eval('setups.Frost'))

    def test_rename_and_paginate(self):
        lua = self.lua()
        lua.execute("for i=1,7 do zProfiles:SaveSetup('Setup '..i) end; zProfiles:ToggleSetups(); Click(zProfiles.setupNext)")
        self.assertEqual(lua.eval('zProfiles.setupRows[1].setupName'), 'Setup 6')
        lua.execute("Click(zProfiles.setupRows[1]); zProfiles.setupNameInput:SetText('Fire'); Click(zProfiles.setupRename)")
        self.assertEqual(lua.eval('zProfiles.selectedSetup'), 'Fire')
        self.assertIsNotNone(lua.eval('setups.Fire'))
        self.assertIsNone(lua.eval('setups["Setup 6"]'))

    def test_talent_button_attaches_after_load_and_uses_parent_visibility(self):
        lua = self.lua()
        lua.execute("TalentFrame=CreateFrame('Frame','TalentFrame'); zProfiles:AttachTalentSetups(); Click(zProfiles.talentSetupsButton)")
        self.assertTrue(lua.eval('zProfiles.setupWindow.parent == TalentFrame'))
        self.assertTrue(lua.eval('zProfiles.setupWindow.shown'))
        lua.execute('zProfiles.setupWindow:Hide(); SlashCmdList.ZPROFILES("bars")')
        self.assertTrue(lua.eval('zProfiles.setupWindow.parent == UIParent'))

    def test_slash_command_opens_panel_when_its_talent_parent_is_hidden(self):
        lua = self.lua()
        lua.execute("TalentFrame=CreateFrame('Frame','TalentFrame'); zProfiles:ToggleSetups(true); TalentFrame:Hide(); SlashCmdList.ZPROFILES('bars')")
        self.assertTrue(lua.eval('zProfiles.setupWindow.parent == UIParent'))
        self.assertTrue(lua.eval('zProfiles.setupWindow:IsVisible()'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
