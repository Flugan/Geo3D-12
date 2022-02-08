// Shaders.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

vector<string> enumerateFiles(string pathName, string filter = "") {
	vector<string> files;
	WIN32_FIND_DATAA FindFileData;
	HANDLE hFind;
	string sName = pathName;
	sName.append(filter);
	hFind = FindFirstFileA(sName.c_str(), &FindFileData);
	if (hFind != INVALID_HANDLE_VALUE)	{
		string fName = pathName;
		fName.append(FindFileData.cFileName);
		files.push_back(fName);
		while (FindNextFileA(hFind, &FindFileData)) {
			fName = pathName;
			fName.append(FindFileData.cFileName);
			files.push_back(fName);
		}
		FindClose(hFind);
	}
	return files;
}

int _tmain(int argc, _TCHAR* argv[])
{
	int test = sizeof(UINT);
	vector<string> gameNames;
	string pathName;
	vector<string> files;
	FILE* f;
	char gamebuffer[10000];
	/*
	DWORD  dBad = 0x00A07E16;
	DWORD dGood = 0x04207E16;
	token_operand* tBad  = (token_operand*)&dBad;
	token_operand* tGood = (token_operand*)&dGood;

	int i = 5 + 5;
	*/
	vector<string> lines;
	fopen_s(&f, "gamelist.txt", "rb");
	if (f) {
		int fr = ::fread(gamebuffer, 1, 10000, f);
		fclose(f);
		lines = stringToLines(gamebuffer, fr);
	}
	if (lines.size() > 0) {
		for (auto i = lines.begin(); i != lines.end(); i++) {
			gameNames.push_back(*i);
		}
	}
	for (DWORD i = 0; i < gameNames.size(); i++) {
		string gameName = gameNames[i];
		if (gameName[0] == ';')
			continue;
		cout << gameName << ":" << endl;

		pathName = gameName;
		pathName.append("\\ShaderCache\\");
		files = enumerateFiles(pathName, "????????????????-??.bin");
		if (files.size() > 0) {
			cout << "bin->asm: ";
#pragma omp parallel
#pragma omp for
			for (int i = 0; i < files.size(); i++) {
				string fileName = files[i];
				vector<byte> ASM;
				ASM = disassembler(readFile(fileName));
				if (ASM.size() > 0) {
					fileName.erase(fileName.size() - 3, 3);
					fileName.append("txt");
					FILE* f;
					fopen_s(&f, fileName.c_str(), "wb");
					fwrite(ASM.data(), 1, ASM.size(), f);
					fclose(f);
				}
				else {
					cout << endl << fileName;
				}
			}
			cout << endl;
		}

		pathName = gameName;
		pathName.append("\\ShaderCache\\");
		files = enumerateFiles(pathName, "????????????????-??.txt");
		if (files.size() > 0) {
			cout << "asm->cbo: ";
#pragma omp for
			for (int i = 0; i < files.size(); i++) {
				string fileName = files[i];

				auto ASM = readFile(fileName);
				if (ASM[0] == ';') {
					cout << endl << fileName;
				}

				fileName.erase(fileName.size() - 3, 3);
				fileName.append("bin");
				auto BIN = readFile(fileName);
				if (BIN.size() > 0) {
					auto CBO = assembler(ASM, BIN);
					if (CBO.size() > 0) {
						fileName.erase(fileName.size() - 3, 3);
						fileName.append("cbo");
						FILE* f;
						fopen_s(&f, fileName.c_str(), "wb");
						fwrite(CBO.data(), 1, CBO.size(), f);
						fclose(f);
					}
				}
			}
			cout << endl;
		}

		pathName = gameName;
		pathName.append("\\ShaderCache\\");
		files = enumerateFiles(pathName, "????????????????-??.cbo");
		if (files.size() > 0) {
			cout << "bin==cbo: ";
#pragma omp for
			for (int i = 0; i < files.size(); i++) {
				string fileName = files[i];

				auto CBO = readFile(fileName);

				fileName.erase(fileName.size() - 3, 3);
				fileName.append("bin");
				auto BIN = readFile(fileName);

				bool valid = true;
				if (CBO.size() == BIN.size()) {
					for (size_t i = 0; i < CBO.size(); i++) {
						if (CBO[i] != BIN[i]) {
							valid = false;
							break;
						}
					}
				}
				else {
					valid = false;
				}
				if (!valid) {
					cout << endl << "Invalid: " << fileName;
					auto ASM2 = disassembler(CBO);
					if (ASM2.size() > 0) {
						fileName.erase(fileName.size() - 3, 3);
						fileName.append("fail");
						FILE* f;
						fopen_s(&f, fileName.c_str(), "wb");
						fwrite(ASM2.data(), 1, ASM2.size(), f);
						fclose(f);
					}
				}
			}
			cout << endl;
		}
	}
	writeLUT();
	cout << endl;
	return 0;
}