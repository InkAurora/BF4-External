#include <iostream>
#include <future>
#include <sstream>
#include <windows.h>
#include <stdlib.h>
#include <vector>

#pragma warning(disable : 4996)

using namespace std;

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);

HHOOK keyboardHook;

int PROCESS_ID;

int GameContext, ClientPlayerManager, ClientPlayer, IdToPlayerMapOffset;

float HipRecoil(NULL), ADSRecoil(NULL);

HANDLE hRead, hWrite;

BOOL B_MINIMAPSPOT(0), B_NORECOIL(0), B_NOSPREAD(0);

class WeaponData {
public:
	float ADSRecoil;
	float HipRecoil;
	float WeaponSpread;
	int Ammo;
	int AmmoInMag;
	string Name;
	BOOL successRead = false;
};

vector<WeaponData> WeaponList;
vector<WeaponData> BackupWeaponList;

DWORD GetProcessPID() {
	DWORD* pid = new DWORD;

	HWND hWnd = FindWindow(NULL, L"Battlefield 4");
	GetWindowThreadProcessId(hWnd, pid);

	return *pid;
}

int RPM(unsigned int address, SIZE_T size = 4) {
	LPCVOID param = (LPCVOID)address;
	if (hRead == NULL) {
		cout << "OpenProcess failed. Error = " << dec << GetLastError() << endl;
		return 0;
	}

	int result, unsignedResult;
	BOOL success = ReadProcessMemory(hRead, (LPCVOID)param, &result, size, NULL);

	unsignedResult = (unsigned int)result;
	return unsignedResult;
}

int RPMLong(unsigned long long address, SIZE_T size = 4) {
	if (hRead == NULL) {
		cout << "OpenProcess failed. Error = " << dec << GetLastError() << endl;
		return 0;
	}

	int result;
	ReadProcessMemory(hRead, (LPCVOID)address, &result, size, NULL);

	return result;
}

char RPMChar(unsigned int address, SIZE_T size = 1) {
	LPCVOID param = (LPCVOID)address;
	if (hRead == NULL) {
		cout << "OpenProcess failed. Error = " << dec << GetLastError() << endl;
		return 0;
	}

	char result;
	ReadProcessMemory(hRead, (LPCVOID)param, &result, size, NULL);

	return result;
}

float RPMFloat(unsigned int address, SIZE_T size = 4) {
	LPCVOID param = (LPCVOID)address;
	if (hRead == NULL) {
		cout << "OpenProcess failed. Error = " << dec << GetLastError() << endl;
		return 0;
	}

	float result;
	ReadProcessMemory(hRead, (LPCVOID)param, &result, size, NULL);

	return result;
}

string ReadString(unsigned int address, size_t size) {
	LPCVOID param = (LPCVOID)address;

	hRead = OpenProcess(PROCESS_VM_READ, FALSE, PROCESS_ID);

	char *str = new char[size];
	ReadProcessMemory(hRead, param, str, size, NULL);

	string temp(&str[0], &str[size]);
	char* t_str = strtok(&temp[0], "\0");
	if (t_str != nullptr) return t_str;
	return string("");
}

int WPM(int value, unsigned int address, SIZE_T size = 4) {
	if (hWrite == NULL) {
		cout << "OpenProcess failed. Error = " << dec << GetLastError() << endl;
		return 0;
	}

	WriteProcessMemory(hWrite, (LPVOID)address, &value, size, NULL);


	return 1;
}

int WPMFloat(float value, unsigned int address, SIZE_T size = 4) {
	if (hWrite == NULL) {
		cout << "OpenProcess failed. Error = " << dec << GetLastError() << endl;
		return 0;
	}

	WriteProcessMemory(hWrite, (LPVOID)address, &value, size, NULL);

	return 1;
}

BOOL Valid(int pointer) {
	return ((unsigned int)pointer >= 0x10000 && (unsigned int)pointer <= 0x0FF000000000000);
};

void readData() {
	hRead = OpenProcess(PROCESS_VM_READ, FALSE, PROCESS_ID);
	GameContext = RPMLong(0x142670D80);
	if (Valid(GameContext)) {
		ClientPlayerManager = RPM(GameContext + 0x60);
		IdToPlayerMapOffset = RPM(ClientPlayerManager + 0x548);
	}
}

void minimapSpot() {
	int pPlayer, pCharacter, pSoldier, spottingTarget, spotType;

	hRead = OpenProcess(PROCESS_VM_READ, FALSE, PROCESS_ID);
	hWrite = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PROCESS_ID);
	for (int i = 0; i < 70; i++) {
		pPlayer = RPM(IdToPlayerMapOffset + (i * 0x8));
		if (Valid(pPlayer)) {
			pCharacter = RPM(pPlayer + 0x14b0);
			if (Valid(pCharacter)) {
				pSoldier = RPM(pCharacter);
				if (Valid(pSoldier)) {
					spottingTarget = RPM(pSoldier + 0xbe8);
					if (Valid(spottingTarget)) {
						spotType = RPMChar(spottingTarget + 0x50);
						if (spotType != 1) WPM(1, spottingTarget + 0x50, 1);
					}
				}
			}
		}
	}
}

int findWeapon(string weaponName) {
	if (WeaponList.size() == 0) return -1;
	for (int i = 0; i < WeaponList.size(); i++) {
		if (WeaponList[i].Name == weaponName) {
			return i;
		}
	}

	return -1;
}

int getWeaponPointer() {
	int ClientPlayer, Character, ClientSoldier, SoldierWeaponComponent, ActiveSlot, CurrentAnimatedWeaponHandler, CurrentWeapon;

	hRead = OpenProcess(PROCESS_VM_READ, FALSE, PROCESS_ID);

	ClientPlayer = RPM(ClientPlayerManager + 0x540);
	Character = RPM(ClientPlayer + 0x14b0);
	ClientSoldier = RPM(Character);
	SoldierWeaponComponent = RPM(ClientSoldier + 0x568);
	CurrentAnimatedWeaponHandler = RPM(SoldierWeaponComponent + 0x890);
	ActiveSlot = RPM(SoldierWeaponComponent + 0xa98);
	CurrentWeapon = RPM(CurrentAnimatedWeaponHandler + (ActiveSlot * 0x8));

	return CurrentWeapon;
}

WeaponData getWeaponData(int ptr = 0) {
	int CurrentWeapon, CFiring, WeaponSway, WeaponSwayData, WeaponName, CurrentWeaponName;

	hRead = OpenProcess(PROCESS_VM_READ, FALSE, PROCESS_ID);

	CurrentWeapon = ptr;
	if (!ptr) CurrentWeapon = getWeaponPointer();
	WeaponData myWeapon;
	if (CurrentWeapon == 0xcccccccc || !Valid(CurrentWeapon)) return myWeapon;
	if (Valid(CurrentWeapon)) {
		CFiring = RPM(CurrentWeapon + 0x49c0);
		WeaponSway = RPM(CFiring + 0x78);
		WeaponSwayData = RPM(WeaponSway + 0x8);
		WeaponName = RPM(CurrentWeapon + 0x30);
		CurrentWeaponName = RPM(WeaponName + 0x130);
		myWeapon.Ammo = RPM(CFiring + 0x1a0);
		myWeapon.AmmoInMag = RPM(CFiring + 0x1a4);
		myWeapon.ADSRecoil = RPMFloat(WeaponSwayData + 0x444);
		myWeapon.HipRecoil = RPMFloat(WeaponSwayData + 0x440);
		myWeapon.WeaponSpread = RPMFloat(WeaponSwayData + 0x430);
		myWeapon.Name = ReadString(CurrentWeaponName, 100);
		myWeapon.successRead = true;

		if (myWeapon.successRead) {
			if (findWeapon(myWeapon.Name) == -1) {
				WeaponList.push_back(myWeapon);
				BackupWeaponList.push_back(myWeapon);
			} else {
				int i = findWeapon(myWeapon.Name);
				WeaponList[i].ADSRecoil = myWeapon.ADSRecoil;
				WeaponList[i].Ammo = myWeapon.Ammo;
				WeaponList[i].AmmoInMag = myWeapon.AmmoInMag;
				WeaponList[i].HipRecoil = myWeapon.HipRecoil;
				WeaponList[i].WeaponSpread = myWeapon.WeaponSpread;
			}
		}

		return myWeapon;
	}

	return myWeapon;
}

void noRecoil(BOOL removeRecoil) {
	int CurrentWeapon, CFiring, WeaponSway, WeaponSwayData;

	hWrite = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PROCESS_ID);

	CurrentWeapon = getWeaponPointer();
	CFiring = RPM(CurrentWeapon + 0x49c0);
	WeaponSway = RPM(CFiring + 0x78);
	WeaponSwayData = RPM(WeaponSway + 0x8);
	if (removeRecoil) {
		WPMFloat(0, WeaponSwayData + 0x444);
		WPMFloat(100, WeaponSwayData + 0x440);
	} else {
		WeaponData myWeapon = getWeaponData(CurrentWeapon);
		if (!myWeapon.successRead) return;
		int i = findWeapon(myWeapon.Name);
		WPMFloat(BackupWeaponList[i].ADSRecoil, WeaponSwayData + 0x444);
		WPMFloat(BackupWeaponList[i].HipRecoil, WeaponSwayData + 0x440);
	}
}

void noSpread(BOOL removeSpread) {
	int CurrentWeapon, CFiring, WeaponSway, WeaponSwayData;

	hWrite = OpenProcess(PROCESS_ALL_ACCESS, FALSE, PROCESS_ID);

	CurrentWeapon = getWeaponPointer();
	CFiring = RPM(CurrentWeapon + 0x49c0);
	WeaponSway = RPM(CFiring + 0x78);
	WeaponSwayData = RPM(WeaponSway + 0x8);
	if (removeSpread) {
		WPMFloat(0, WeaponSwayData + 0x430);
		WPMFloat(0, WeaponSwayData + 0x434);
		WPMFloat(0, WeaponSwayData + 0x438);
		WPMFloat(0, WeaponSwayData + 0x43c);
	} else {
		WPMFloat(1, WeaponSwayData + 0x430);
		WPMFloat(1, WeaponSwayData + 0x434);
		WPMFloat(1, WeaponSwayData + 0x438);
		WPMFloat(1, WeaponSwayData + 0x43c);
	}
}

void updateWeapon() {
	WeaponData myWeapon = getWeaponData();
	if (!myWeapon.successRead) return;
	if (B_NORECOIL) {
		if (myWeapon.ADSRecoil != 0 || myWeapon.HipRecoil != 100) {
			noRecoil(true);
		}
	} else {
		if (myWeapon.ADSRecoil == 0 && myWeapon.HipRecoil == 100) {
			noRecoil(false);
		}
	}
	if (B_NOSPREAD) {
		if (myWeapon.WeaponSpread == 1) {
			noSpread(true);
		}
	} else {
		if (myWeapon.WeaponSpread == 0) {
			noSpread(false);
		}
	}
}

void mainFlow() {
	while (1) {
		if (B_MINIMAPSPOT) minimapSpot();
		updateWeapon();
		Sleep(16);
	}
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
	PKBDLLHOOKSTRUCT key = (PKBDLLHOOKSTRUCT)lParam;
	string message;

	if (wParam == WM_KEYDOWN && nCode == HC_ACTION) {
		switch (key->vkCode) {
		case VK_NUMPAD1:
			B_NORECOIL = !B_NORECOIL;
			message = B_NORECOIL ? "No Recoil Enabled!" : "No Recoil Disabled!";
			cout << message << endl;
			break;
		case VK_NUMPAD2:
			B_NOSPREAD = !B_NOSPREAD;
			message = B_NOSPREAD ? "No Spread Enabled!" : "No Spread Disabled!";
			cout << message << endl;
			break;
		case VK_NUMPAD7:
			B_MINIMAPSPOT = !B_MINIMAPSPOT;
			message = B_MINIMAPSPOT ? "Minimap Spotting Enabled!" : "Minimap Spotting Disabled!";
			cout << message << endl;
			break;
		default:
			break;
		}
	}

	return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

int main() {

	cout << "BF4 External                             ;by INK" << endl;

	PROCESS_ID = GetProcessPID();

	keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, 0, 0);

	readData();

	future<void> get = async(launch::async, mainFlow);

	MSG msg = { 0 };
	while (GetMessage(&msg, NULL, 0, 0) != 0);
	UnhookWindowsHookEx(keyboardHook);

	return 0;
}
