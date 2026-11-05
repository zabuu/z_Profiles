"""Ring-slot snapshots, persistence payloads, rollback and optional real-zPie integration."""
from pathlib import Path
import unittest
import test_setups

ROOT = Path(__file__).resolve().parents[1]
PIE = r'''
macroIndices = {Burst=7}
function GetMacroIndexByName(name) return macroIndices[name] or 0 end
function GetMacroInfo(index)
  for name, value in pairs(macroIndices) do if index == value then return name,'MacroIcon','/cast Frostbolt' end end
end
zPie = {rings={}, spellCache={}, characterProfileKey='Char1 - Realm'}
function zPie:GetRingCount() return zPieDB.ringCount end
function zPie:CreateRingData(index) return {name='Ring '..index,radius=75,anchor='MOUSE',items={}} end
function zPie:InitButtons()
  if state.pieRefreshFailure then state.pieRefreshFailure=false; error('ring refresh failed') end
  self.initialized = (self.initialized or 0) + 1
end
function zPie:RefreshDBIcons()
  for ring=1,self:GetRingCount() do
    for slot,item in pairs(zPieDB[ring].items) do
      if item.type=='MACRO' and item.macroIndex then
        local name,icon=GetMacroInfo(item.macroIndex)
        item.name=name or item.name
      end
    end
  end
end
zPieDB={ringCount=11,arrowStyle='CustomArrow',showSelectionNames=false}
for i=1,11 do zPieDB[i]={name='My ring '..i,radius=123,anchor='CENTER',tapBehavior='READY',items={}} end
zPieDB[1].items[1]={type='SPELL_OR_ITEM',name='Frostbolt',icon='FrostIcon'}
zPieDB[1].items[3]={type='MACRO',name='Burst',icon='MacroIcon',macroIndex=7}
zPieDB[1].items[8]={type='RAID_MARKER',name='Skull',markerIndex=8,icon='RaidIcon'}
zPieDB[11].items[8]={type='SUPER_MACRO',name='Long macro\nwith a tab\tand UTF8 é',icon='SuperIcon'}
zPieDB[2].items[4]={type='SPELL_OR_ITEM',name='Hearthstone'}
zPieDB[11].binding='ALT-F9'
zPieProfilesDB={characters={['Char1 - Realm']=zPieDB},saved={Keep={name='Unrelated'}}}
zPieConfigFrame={Refresh=function(self) self.refreshed=true end}
'''


class PieTests(unittest.TestCase):
    def lua(self):
        lua = test_setups.SetupTests().lua()
        lua.execute(PIE)
        return lua

    def test_save_apply_restores_sparse_slots_and_clears_new_assignments(self):
        lua = self.lua()
        lua.execute('''
          zProfiles:SaveSetup('Frost')
          zPieDB[1].items={}; zPieDB[11].items={}; zPieDB[2].items[8]={name='Extra'}
          zProfiles:ApplySetup('Frost')
        ''')
        self.assertEqual(lua.eval('zPieDB[1].items[1].name'), 'Frostbolt')
        self.assertEqual(lua.eval('zPieDB[1].items[3].name'), 'Burst')
        self.assertEqual(lua.eval('zPieDB[1].items[8].markerIndex'), 8)
        self.assertEqual(lua.eval('zPieDB[2].items[4].name'), 'Hearthstone')
        self.assertIn('UTF8 é', lua.eval('zPieDB[11].items[8].name'))
        self.assertIsNone(lua.eval('zPieDB[2].items[8]'))
        self.assertEqual(lua.eval('reloads'), 0)
        self.assertTrue(lua.eval('zPieConfigFrame.refreshed'))

    def test_slots_are_independent_copies_and_undo_restores_unsaved_assignments(self):
        lua = self.lua()
        lua.execute('''
          zProfiles:SaveSetup('Frost')
          zPieDB[1].items[1].name='Unsaved Fireball'; zPieDB[1].items[3]=nil
          zProfiles:ApplySetup('Frost')
          zPieDB[1].items[1].name='Changed after apply'
          zProfiles:ApplySetup(nil,true)
        ''')
        self.assertEqual(lua.eval('zPieDB[1].items[1].name'), 'Unsaved Fireball')
        self.assertIsNone(lua.eval('zPieDB[1].items[3]'))
        self.assertEqual(lua.eval('zProfiles:ParsePie(setups.Frost.pie).rings[1][1].name'), 'Frostbolt')

    def test_layout_bindings_character_store_and_other_profiles_are_preserved(self):
        lua = self.lua()
        lua.execute("zProfiles:SaveSetup('Frost'); zPieDB[1].radius=150; zPieDB[1].name='Renamed'; zProfiles:ApplySetup('Frost')")
        self.assertEqual(lua.eval('zPieDB[1].radius'), 150)
        self.assertEqual(lua.eval('zPieDB[1].name'), 'Renamed')
        self.assertEqual(lua.eval('zPieDB[11].binding'), 'ALT-F9')
        self.assertEqual(lua.eval('zPieDB.arrowStyle'), 'CustomArrow')
        self.assertFalse(lua.eval('zPieDB.showSelectionNames'))
        self.assertTrue(lua.eval('zPieProfilesDB.characters["Char1 - Realm"] == zPieDB'))
        self.assertEqual(lua.eval('zPieProfilesDB.saved.Keep.name'), 'Unrelated')

    def test_macro_names_follow_reindexed_macros_and_missing_macros_do_not_change_name(self):
        lua = self.lua()
        lua.execute("zProfiles:SaveSetup('Frost'); macroIndices={Burst=19,Wrong=7}; zProfiles:ApplySetup('Frost')")
        self.assertEqual(lua.eval('zPieDB[1].items[3].macroIndex'), 19)
        self.assertEqual(lua.eval('zPieDB[1].items[3].name'), 'Burst')
        lua.execute("macroIndices={Wrong=7}; zProfiles:ApplySetup('Frost')")
        self.assertEqual(lua.eval('zPieDB[1].items[3].name'), 'Burst')
        self.assertIsNone(lua.eval('zPieDB[1].items[3].macroIndex'))

    def test_native_and_binding_failures_restore_previous_ring_assignments(self):
        for fault in ('state.applyFailure=true', 'state.saveFailure=true', 'state.badKey="W"'):
            with self.subTest(fault=fault):
                lua = self.lua()
                lua.execute("zProfiles:SaveSetup('Frost'); zPieDB[1].items[1].name='Fireball'; bindings={Q='ACTIONBUTTON1'}; currentBars='Fire'; " + fault + "; zProfiles:ApplySetup('Frost')")
                self.assertEqual(lua.eval('zPieDB[1].items[1].name'), 'Fireball')
                self.assertEqual(lua.eval('currentBars'), 'Fire')
                self.assertEqual(lua.eval('bindings.Q'), 'ACTIONBUTTON1')

    def test_ring_refresh_failure_rolls_back_before_native_bar_application(self):
        lua = self.lua()
        lua.execute("zProfiles:SaveSetup('Frost'); zPieDB[1].items[1].name='Fireball'; currentBars='Fire'; state.pieRefreshFailure=true; zProfiles:ApplySetup('Frost')")
        self.assertEqual(lua.eval('zPieDB[1].items[1].name'), 'Fireball')
        self.assertEqual(lua.eval('currentBars'), 'Fire')
        self.assertIn('ring refresh failed', lua.eval('zProfiles.setupNotice'))

    def test_disabled_or_busy_pie_blocks_before_bindings_undo_or_bars_change(self):
        for unavailable in ('zPie=nil', 'zPie.currentRing=1', 'zPie.pendingItem={}', 'zPieDB=nil'):
            with self.subTest(unavailable=unavailable):
                lua = self.lua()
                lua.execute("zProfiles:SaveSetup('Frost'); currentBars='Fire'; bindings={Q='MOVEFORWARD'}; " + unavailable + "; zProfiles:ApplySetup('Frost')")
                self.assertEqual(lua.eval('currentBars'), 'Fire')
                self.assertEqual(lua.eval('bindings.Q'), 'MOVEFORWARD')
                self.assertIsNone(lua.eval('undoSetup'))

    def test_legacy_setups_leave_rings_unchanged_and_update_adds_them(self):
        lua = self.lua()
        lua.execute("zProfiles:SaveSetup('Frost'); setups.Frost.pie=nil; zPieDB[1].items[1].name='Fireball'; zProfiles:ApplySetup('Frost')")
        self.assertEqual(lua.eval('zPieDB[1].items[1].name'), 'Fireball')
        self.assertIn('Update', lua.eval('zProfiles.setupNotice'))
        lua.execute("zProfiles:SaveSetup('Frost',true); zPieDB[1].items[1]=nil; zProfiles:ApplySetup('Frost')")
        self.assertEqual(lua.eval('zPieDB[1].items[1].name'), 'Fireball')

    def test_empty_rings_are_saved_and_clear_all_slots_including_extra_rings(self):
        lua = self.lua()
        lua.execute('''
          for i=1,11 do zPieDB[i].items={} end
          zProfiles:SaveSetup('Empty'); zPieDB[1].items[1]={name='Fireball'}
          zPieDB[12]=zPie:CreateRingData(12); zPieDB[12].items[8]={name='Extra'}; zPieDB.ringCount=12
          zProfiles:ApplySetup('Empty')
        ''')
        self.assertIsNone(lua.eval('next(zPieDB[1].items)'))
        self.assertIsNone(lua.eval('next(zPieDB[12].items)'))
        self.assertEqual(lua.eval('zPieDB.ringCount'), 12)

    def test_corrupt_or_executable_payloads_are_rejected_before_mutation(self):
        for value in ('return os.execute("bad")', 'ZPR1\n0\n', 'ZPR1\n257\n',
                      'ZPR1\n1\n1\t9\tMACRO\t41\t\t1\t0\n',
                      'ZPR1\n1\n1\t1\tMACRO\t00\t\t1\t0\n',
                      'ZPR1\n1\n1\t1\tMACRO\t41\t\t1\t0\n1\t1\tMACRO\t42\t\t2\t0\n'):
            with self.subTest(value=value):
                lua = self.lua()
                lua.execute("zProfiles:SaveSetup('Frost')")
                lua.globals().bad = value
                lua.execute("setups.Frost.pie=bad; zProfiles:ApplySetup('Frost')")
                self.assertIsNone(lua.eval('undoSetup'))
                self.assertEqual(lua.eval('zPieDB[1].items[1].name'), 'Frostbolt')

    def test_reopen_addon_can_apply_native_payload_without_previous_lua_snapshot(self):
        first = self.lua()
        first.execute("zProfiles:SaveSetup('Frost')")
        second = self.lua()
        second.globals().stored_pie = first.eval('setups.Frost.pie')
        second.globals().stored_bindings = first.eval('setups.Frost.bindings')
        second.execute("setups.Frost={pie=stored_pie,bindings=stored_bindings,bars='Saved bars'}; zPieDB[1].items={}; zProfiles:ApplySetup('Frost')")
        self.assertEqual(second.eval('zPieDB[1].items[1].name'), 'Frostbolt')
        self.assertEqual(second.eval('currentBars'), 'Saved bars')

    def test_adapter_with_installed_zpie_runtime(self):
        source = ROOT.parent / 'z_Pie' / 'zPie.lua'
        if not source.exists():
            self.skipTest('Optional installed zPie runtime is absent')
        lua = self.lua()
        lua.execute('function setglobal(k,v) _G[k]=v end')
        lua.execute(source.read_text(encoding='utf-8'))
        lua.execute('''
          local mt=getmetatable(zPie)
          local method=mt.__index
          mt.__index=function(t,k)
            if string.find(k,'^[A-Z]') then return method(t,k) end
          end
          zPie.characterProfileKey='Char1 - Realm'
          zProfiles:SaveSetup('Frost'); zPieDB[1].items={}
          zProfiles:ApplySetup('Frost')
        ''')
        self.assertEqual(lua.eval('zPieDB[1].items[3].name'), 'Burst')
        self.assertEqual(lua.eval('zPieDB[11].items[8].type'), 'SUPER_MACRO')
        self.assertTrue(lua.eval('zPie.rings[11][8] ~= nil'))
        self.assertTrue(lua.eval('zPieProfilesDB.characters["Char1 - Realm"] == zPieDB'))


if __name__ == '__main__':
    unittest.main(verbosity=2)
