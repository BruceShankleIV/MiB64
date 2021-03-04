#include <Windows.h>
#include <stdio.h>
#include "RomTools_Common.h"
#include "Pif.h"

void CountryCodeToString (char string[], BYTE Country, int length) {
	switch (Country) {
		case '7': strncpy(string, "Beta", length); break;
		case 'A': strncpy(string, "NTSC", length); break;
		case 'B': strncpy(string, "Brazil", length); break;
		case 'D': strncpy(string, "Germany", length); break;
		case 'E': strncpy(string, "USA", length); break;
		case 'F': strncpy(string, "France", length); break;
		case 'J': strncpy(string, "Japan", length); break;
		case 'I': strncpy(string, "Italy", length); break;
		case 'P': strncpy(string, "Europe", length); break;
		case 'S': strncpy(string, "Spain", length); break;
		case 'U': strncpy(string, "Australia", length); break;
		case 'X': strncpy(string, "PAL", length); break;
		case 'Y': strncpy(string, "PAL", length); break;
		case ' ': strncpy(string, "None (PD by NAN)", length); break;
		case 0: strncpy(string, "None (PD)", length); break;
		default:
			if (length > 20)
				sprintf(string, "Unknown %c (%02X)", Country, Country);
			break;
	}
}

int GetRomRegion (BYTE *RomData) {
	BYTE Country;

	GetRomCountry(&Country, RomData);

	switch(Country)
	{
		case 0x44: // Germany
		case 0x46: // French
		case 0x49: // Italian
		case 0x50: // Europe
		case 0x53: // Spanish
		case 0x55: // Australia
		case 0x58: // X (PAL)
		case 0x59: // X (PAL)
			return PAL_Region;
	
		case 0x37:	// 7 (Beta)
		case 0x41:	// NTSC (Only 1080 JU?)
		case 0x42:	// Brazil
		case 0x45:	// USA
		case 0x4A:	// Japan
		case 0x20:	// None (PD)
		case 0x0:	// None (PD)
			return NTSC_Region;

		default:
			return Unknown_Region;
	}
}

void GetRomName (char *Name, BYTE *RomData) {
	int count;
	
	memcpy(Name, (void *)(RomData + 0x20), 20);
	for(count = 0 ; count < 20; count += 4) {
		Name[count] ^= Name[count+3];
		Name[count + 3] ^= Name[count];
		Name[count] ^= Name[count+3];
		Name[count + 1] ^= Name[count + 2];
		Name[count + 2] ^= Name[count + 1];
		Name[count + 1] ^= Name[count + 2];
	}
	Name[21] = '\0';
}

void GetRomCartID (char *ID, BYTE *RomData) { 
	ID[0] = *(RomData + 0x3F);
	ID[1] = *(RomData + 0x3E);
	ID[2] = '\0';
}

void GetRomManufacturer (BYTE *Manufacturer, BYTE *RomData) {
	*Manufacturer = *(BYTE *)(RomData + 0x38);
}

void GetRomCountry (BYTE *Country, BYTE *RomData) {
	*Country = *(RomData + 0x3D);
}

void GetRomCRC1 (DWORD *Crc1, BYTE *RomData) { 
	*Crc1 = *(DWORD *)(RomData + 0x10);
}

void GetRomCRC2 (DWORD *Crc2, BYTE *RomData) { 
	*Crc2 = *(DWORD *)(RomData + 0x14);
}

int GetRomCicChipID (BYTE *RomData) {
	return GetCicChipID(RomData);
}

void RomID (char *ID, BYTE *RomData) {
	DWORD CRC1, CRC2;
	BYTE Country;

	GetRomCRC1(&CRC1, RomData);
	GetRomCRC2(&CRC2, RomData);
	GetRomCountry(&Country, RomData);

	RomIDPreScanned(ID, &CRC1, &CRC2, &Country);
}

void RomIDPreScanned (char *ID, DWORD *CRC1, DWORD *CRC2, BYTE *Country) {
	sprintf(ID, "%08X-%08X-C:%02X", *CRC1, *CRC2, *Country);
}