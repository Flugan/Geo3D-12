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
		cout << gameName << endl;

		pathName = gameName;
		pathName.append("ShaderCache\\");
		auto newFiles = enumerateFiles(pathName, "????????????????-??.bin");
		files.insert(files.end(), newFiles.begin(), newFiles.end());
	}

#pragma omp parallel
#pragma omp for
	for (int i = 0; i < files.size(); i++) {
		string fileName = files[i];
		auto BIN = readFile(fileName);
		auto ASM = disassembler(BIN);
		vector<byte> CBO = assembler(ASM, BIN);
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
		/*
		if (ASM[0] == ';') {
			string ASMfilename = fileName;
			ASMfilename.erase(fileName.size() - 3, 3);
			ASMfilename.append("dxil.txt");
			fopen_s(&f, ASMfilename.c_str(), "wb");
			fwrite(ASM.data(), 1, ASM.size(), f);
			fclose(f);
			continue;
		}
		else {
			string ASMfilename = fileName;
			ASMfilename.erase(fileName.size() - 3, 3);
			ASMfilename.append("txt");
			fopen_s(&f, ASMfilename.c_str(), "wb");
			fwrite(ASM.data(), 1, ASM.size(), f);
			fclose(f);
		}

		fileName.erase(fileName.size() - 3, 3);
		fileName.append("fail.txt");
		if (!valid) {
			auto ASM2 = disassembler(CBO);
			if (ASM2.size() > 0) {
				FILE* f;
				fopen_s(&f, fileName.c_str(), "wb");
				fwrite(ASM2.data(), 1, ASM2.size(), f);
				fclose(f);
			}
		}
		else {
			DeleteFileA(fileName.c_str());
		}
		*/
	}
	writeLUT();
	cout << endl;
	return 0;
}