"""Sidebar interactions and layout lifecycle; no game process is accessed."""
import unittest
import test_setups


class SidebarTests(unittest.TestCase):
    def lua(self):
        lua = test_setups.SetupTests().lua()
        lua.execute("zProfiles:SaveSetup('MM'); currentBars='Survival'; zProfiles:SaveSetup('Survival'); zProfiles:ToggleSetups()")
        return lua

    def test_selection_waits_for_the_single_apply_action(self):
        lua = self.lua()
        lua.execute('Click(zProfiles.setupRows[1])')
        self.assertEqual(lua.eval('currentBars'), 'Survival')
        lua.execute('Click(zProfiles.setupApply)')
        self.assertEqual(lua.eval('currentBars'), 'Frost bars')
        self.assertTrue(lua.eval('zProfiles.setupRows[1].last:IsShown()'))
        self.assertFalse(lua.eval('zProfiles.setupPrevious:IsShown()'))

    def test_new_editor_saves_trimmed_name_and_returns_to_the_list(self):
        lua = self.lua()
        lua.execute("Click(zProfiles.setupNew); zProfiles.setupNameInput:SetText('  PvP  ')")
        self.assertTrue(lua.eval('zProfiles.setupEditor:IsShown()'))
        self.assertFalse(lua.eval('zProfiles.setupApply:IsShown()'))
        self.assertFalse(lua.eval('zProfiles.setupRows[1]:IsShown()'))
        lua.execute('Click(zProfiles.setupSave)')
        self.assertIsNone(lua.eval('zProfiles.setupEditorMode'))
        self.assertEqual(lua.eval('zProfiles.selectedSetup'), 'PvP')
        self.assertTrue(lua.eval('zProfiles.setupApply:IsShown()'))

    def test_duplicate_name_keeps_editor_open_with_error_and_focus(self):
        lua = self.lua()
        lua.execute("Click(zProfiles.setupNew); zProfiles.setupNameInput:SetText('MM'); Click(zProfiles.setupSave)")
        self.assertTrue(lua.eval('zProfiles.setupEditor:IsShown()'))
        self.assertTrue(lua.eval('zProfiles.setupNameInput.focused'))
        self.assertIn('exists', lua.eval('zProfiles.setupNotice'))
        lua.execute('Click(zProfiles.setupCancel)')
        self.assertTrue(lua.eval('zProfiles.setupRows[1]:IsShown()'))

    def test_empty_state_primary_action_opens_first_save(self):
        lua = self.lua()
        lua.execute('setups={}; zProfiles:RefreshSetups(); Click(zProfiles.setupApply)')
        self.assertEqual(lua.eval('zProfiles.setupEditorMode'), 'new')
        self.assertTrue(lua.eval('zProfiles.setupNameInput.focused'))

    def test_menu_rename_prefills_and_confirmation_does_not_delete_early(self):
        lua = self.lua()
        lua.execute('Click(zProfiles.setupMore); Click(zProfiles.setupRename)')
        self.assertEqual(lua.eval('zProfiles.setupNameInput:GetText()'), 'Survival')
        lua.execute("zProfiles.setupNameInput:SetText('Tank'); Click(zProfiles.setupSave); Click(zProfiles.setupMore); Click(zProfiles.setupDelete)")
        self.assertIsNotNone(lua.eval('setups.Tank'))
        self.assertEqual(lua.eval('zProfiles.setupDelete:GetText()'), 'Confirm delete')
        lua.execute('Click(zProfiles.setupDelete)')
        self.assertIsNone(lua.eval('setups.Tank'))
        self.assertFalse(lua.eval('zProfiles.setupMenu:IsShown()'))

    def test_paging_selects_visible_row_and_does_not_apply_hidden_selection(self):
        lua = self.lua()
        lua.execute("for i=1,7 do zProfiles:SaveSetup('Setup '..i) end; Click(zProfiles.setupNext)")
        self.assertEqual(lua.eval('zProfiles.selectedSetup'), lua.eval('zProfiles.setupRows[1].setupName'))
        self.assertEqual(lua.eval('currentBars'), 'Survival')
        self.assertTrue(lua.eval('zProfiles.setupPrevious:IsShown()'))

    def test_docks_to_pfui_edges_and_uses_left_side_if_right_is_full(self):
        lua = self.lua()
        lua.execute('''
          TalentFrame=CreateFrame('Frame','TalentFrame',UIParent)
          TalentFrame.backdrop=CreateFrame('Frame',nil,TalentFrame)
          TalentFrame.backdrop.right=600; UIParent.width=1000
          zProfiles:PositionSetupWindow(true)
        ''')
        self.assertEqual(lua.eval('zProfiles.setupDockSide'), 'right')
        self.assertEqual(lua.eval('table.getn(zProfiles.setupWindow.points)'), 2)
        self.assertTrue(lua.eval('zProfiles.setupWindow.points[1][2] == TalentFrame.backdrop'))
        self.assertEqual(lua.eval('zProfiles.setupWindow.points[2][1]'), 'BOTTOMLEFT')
        lua.execute('TalentFrame.backdrop.right=900; zProfiles:PositionSetupWindow(true)')
        self.assertEqual(lua.eval('zProfiles.setupDockSide'), 'left')
        self.assertEqual(lua.eval('zProfiles.setupWindow.points[1][1]'), 'TOPRIGHT')
        self.assertEqual(lua.eval('zProfiles.setupWindow.width'), 168)

    def test_docked_drag_does_not_detach_and_standalone_can_move(self):
        lua = self.lua()
        lua.execute("TalentFrame=CreateFrame('Frame','TalentFrame',UIParent); zProfiles:PositionSetupWindow(true); zProfiles.setupWindow.scripts.OnDragStart()")
        self.assertTrue(lua.eval('zProfiles.setupWindow.parent == TalentFrame'))
        self.assertIsNone(lua.eval('rawget(zProfiles.setupWindow,"moving")'))
        lua.execute('zProfiles:PositionSetupWindow(false); zProfiles.setupWindow.scripts.OnDragStart()')
        self.assertTrue(lua.eval('zProfiles.setupWindow.moving'))

    def test_fade_stops_and_hiding_cleans_up_editor_focus_and_menu(self):
        lua = self.lua()
        lua.execute('arg1=.13; zProfiles.setupWindow.scripts.OnUpdate()')
        self.assertIsNone(lua.eval('zProfiles.setupWindow.scripts.OnUpdate'))
        self.assertEqual(lua.eval('zProfiles.setupWindow:GetAlpha()'), 1)
        lua.execute('Click(zProfiles.setupNew); zProfiles.setupWindow:Hide()')
        self.assertFalse(lua.eval('zProfiles.setupNameInput.focused'))
        self.assertIsNone(lua.eval('zProfiles.setupEditorMode'))
        self.assertFalse(lua.eval('zProfiles.setupMenu:IsShown()'))

    def test_long_names_fit_but_keep_the_full_identity(self):
        lua = self.lua()
        lua.execute("zProfiles:SaveSetup(string.rep('W',32)); zProfiles:RenderSetups()")
        self.assertTrue(lua.eval("string.find(zProfiles.setupRows[3]:GetText(),'%.%.%.') ~= nil"))
        self.assertEqual(lua.eval('zProfiles.setupRows[3].setupName'), 'W' * 32)


if __name__ == '__main__':
    unittest.main(verbosity=2)
