#ifndef StageDir
  #define StageDir "release-opengl"
#endif
#ifndef AppVer
  #define AppVer "1.0.0"
#endif
#ifndef OutputPath
  #define OutputPath "."
#endif
#ifndef IconFile
  #define IconFile "..\CSC8503\app.ico"
#endif

[Setup]
AppName=NEUROMANCER
AppVersion={#AppVer}
AppPublisher=CSC8507-08
DefaultDirName={autopf}\NEUROMANCER
DefaultGroupName=NEUROMANCER
UninstallDisplayIcon={app}\CSC8503.exe
OutputDir={#OutputPath}
OutputBaseFilename=NEUROMANCER_Setup
Compression=lzma2/ultra64
SolidCompression=yes
SetupIconFile={#IconFile}
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableProgramGroupPage=yes
DisableDirPage=no
AlwaysShowDirOnReadyPage=yes

[Files]
Source: "{#StageDir}\CSC8503.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\fmod.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#StageDir}\Assets\*"; DestDir: "{app}\Assets"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autodesktop}\NEUROMANCER"; Filename: "{app}\CSC8503.exe"; IconFilename: "{app}\CSC8503.exe"; Comment: "Launch NEUROMANCER"
Name: "{group}\NEUROMANCER"; Filename: "{app}\CSC8503.exe"
Name: "{group}\Uninstall NEUROMANCER"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\CSC8503.exe"; Description: "Launch NEUROMANCER"; Flags: nowait postinstall skipifsilent

[Code]
var
  IntegrityPage: TWizardPage;
  IntegrityLabel: TNewStaticText;
  IntegrityMemo: TNewMemo;
  RepairCheckbox: TNewCheckBox;
  CheckResult: Integer;
  MissingList: String;
  MissingCount: Integer;
  TotalCount: Integer;

procedure AddFile(const RelPath: String);
var
  FullPath: String;
begin
  TotalCount := TotalCount + 1;
  FullPath := ExpandConstant('{app}') + '\' + RelPath;
  if not FileExists(FullPath) then
  begin
    MissingCount := MissingCount + 1;
    if MissingCount <= 20 then
      MissingList := MissingList + '  - ' + RelPath + #13#10
    else if MissingCount = 21 then
      MissingList := MissingList + '  ... and more' + #13#10;
  end;
end;

procedure CheckIntegrity();
begin
  MissingCount := 0;
  TotalCount := 0;
  MissingList := '';

  AddFile('CSC8503.exe');
  AddFile('fmod.dll');
  AddFile('Assets\Audio\BGM\30SecCountdown.mp3');
  AddFile('Assets\Audio\BGM\Fail.mp3');
  AddFile('Assets\Audio\BGM\Hunt.mp3');
  AddFile('Assets\Audio\BGM\MainMenu-BGM.mp3');
  AddFile('Assets\Audio\BGM\Normal.mp3');
  AddFile('Assets\Audio\BGM\Succeed.mp3');
  AddFile('Assets\Audio\SFX\button_press.mp3');
  AddFile('Assets\Audio\SFX\itemGet.mp3');
  AddFile('Assets\Dialogue\Dialogue_Alert_EN.json');
  AddFile('Assets\Dialogue\Dialogue_Exposed_EN.json');
  AddFile('Assets\Dialogue\Dialogue_Normal_EN.json');
  AddFile('Assets\Dialogue\Dialogue_Repair_EN.json');
  AddFile('Assets\Dialogue\Dialogue_Trade_EN.json');
  AddFile('Assets\Fonts\Cousine-Regular.ttf');
  AddFile('Assets\Fonts\Roboto-Medium.ttf');
  AddFile('Assets\Fonts\ZLabsRoundPix_16px_M_CN.ttf');
  AddFile('Assets\GLTF\Orbs\DDOS.gltf');
  AddFile('Assets\GLTF\Orbs\HoloBait.gltf');
  AddFile('Assets\GLTF\Orbs\KeyCard.gltf');
  AddFile('Assets\GLTF\Orbs\Map.gltf');
  AddFile('Assets\GLTF\Orbs\RoomAI.gltf');
  AddFile('Assets\GLTF\Orbs\Target.gltf');
  AddFile('Assets\GLTF\Orbs\player.gltf');
  AddFile('Assets\GLTF\Orbs\enemy.gltf');
  AddFile('Assets\Meshes\TutorialMap.gltf');
  AddFile('Assets\Meshes\TutorialMap.navmesh');
  AddFile('Assets\Meshes\HangerA.gltf');
  AddFile('Assets\Meshes\HangerA.navmesh');
  AddFile('Assets\Meshes\HangerB.gltf');
  AddFile('Assets\Meshes\HangerB.navmesh');
  AddFile('Assets\Meshes\Helipad.gltf');
  AddFile('Assets\Meshes\Helipad.navmesh');
  AddFile('Assets\Meshes\Lab.gltf');
  AddFile('Assets\Meshes\Lab.navmesh');
  AddFile('Assets\Meshes\Dock.gltf');
  AddFile('Assets\Meshes\Dock.navmesh');
  AddFile('Assets\Prefabs\Prefab_Player.json');
  AddFile('Assets\Prefabs\Prefab_Map_TutorialLevel.json');
  AddFile('Assets\Prefabs\Prefab_Map_HangerA.json');
  AddFile('Assets\Prefabs\Prefab_Map_Dock.json');
  AddFile('Assets\Shaders\scene.vert');
  AddFile('Assets\Shaders\scene.frag');
  AddFile('Assets\Shaders\shadow.vert');
  AddFile('Assets\Shaders\shadow.frag');
  AddFile('Assets\Shaders\lit.vert');
  AddFile('Assets\Shaders\lit_pbr.frag');
  AddFile('Assets\Textures\Default.png');
  AddFile('Assets\Textures\Cubemap\skyrender0001.png');

  if not FileExists(ExpandConstant('{app}') + '\CSC8503.exe') then
    CheckResult := 0
  else if MissingCount = 0 then
    CheckResult := 1
  else
    CheckResult := 2;
end;

procedure InitializeWizard();
begin
  IntegrityPage := CreateCustomPage(wpSelectDir,
    'Installation Check', 'Checking existing installation...');

  IntegrityLabel := TNewStaticText.Create(IntegrityPage);
  IntegrityLabel.Parent := IntegrityPage.Surface;
  IntegrityLabel.Left := 0;
  IntegrityLabel.Top := 0;
  IntegrityLabel.Width := IntegrityPage.SurfaceWidth;
  IntegrityLabel.Height := 40;
  IntegrityLabel.AutoSize := False;
  IntegrityLabel.WordWrap := True;

  IntegrityMemo := TNewMemo.Create(IntegrityPage);
  IntegrityMemo.Parent := IntegrityPage.Surface;
  IntegrityMemo.Left := 0;
  IntegrityMemo.Top := 50;
  IntegrityMemo.Width := IntegrityPage.SurfaceWidth;
  IntegrityMemo.Height := 200;
  IntegrityMemo.ReadOnly := True;
  IntegrityMemo.ScrollBars := ssVertical;

  RepairCheckbox := TNewCheckBox.Create(IntegrityPage);
  RepairCheckbox.Parent := IntegrityPage.Surface;
  RepairCheckbox.Left := 0;
  RepairCheckbox.Top := 260;
  RepairCheckbox.Width := IntegrityPage.SurfaceWidth;
  RepairCheckbox.Height := 24;
  RepairCheckbox.Caption := 'Repair installation (replace missing files)';
  RepairCheckbox.Checked := True;
  RepairCheckbox.Visible := False;
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = IntegrityPage.ID then
  begin
    CheckIntegrity();
    case CheckResult of
      0:
      begin
        IntegrityLabel.Caption := 'No existing installation detected. Ready to install NEUROMANCER.';
        IntegrityMemo.Text := 'A fresh installation will be performed.' + #13#10 +
          'Files to install: ' + IntToStr(TotalCount) + ' checked items + all assets.';
        RepairCheckbox.Visible := False;
      end;
      1:
      begin
        IntegrityLabel.Caption := 'Existing installation detected. All files are intact!';
        IntegrityMemo.Text := 'All ' + IntToStr(TotalCount) + ' key files verified successfully.' + #13#10 + #13#10 +
          'Your installation is complete. You can click Cancel to exit,' + #13#10 +
          'or click Next to reinstall/update.';
        RepairCheckbox.Visible := False;
      end;
      2:
      begin
        IntegrityLabel.Caption := 'Existing installation detected, but ' +
          IntToStr(MissingCount) + ' file(s) are missing:';
        IntegrityMemo.Text := MissingList;
        RepairCheckbox.Visible := True;
        RepairCheckbox.Checked := True;
      end;
    end;
  end;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = IntegrityPage.ID then
  begin
    if (CheckResult = 2) and (not RepairCheckbox.Checked) then
    begin
      if MsgBox('Skip repair and exit setup?', mbConfirmation, MB_YESNO) = IDYES then
      begin
        WizardForm.Close;
        Result := False;
      end
      else
        Result := False;
    end;
  end;
end;
