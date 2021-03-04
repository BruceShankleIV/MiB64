#ifndef ROMTOOLS_H
#define ROMTOOLS_H

#define Unknown_Region 0
#define NTSC_Region 1
#define PAL_Region 2

void CountryCodeToString (char string[], BYTE Country, int length);
int GetRomRegion (BYTE *Country);
void GetRomName (char *Name, BYTE *RomData);
void GetRomCartID (char *ID, BYTE *RomData);
void GetRomManufacturer (BYTE *Manufacturer, BYTE *RomData);
void GetRomCountry (BYTE *Country, BYTE *RomData);
void GetRomCRC1 (DWORD *Crc1, BYTE *RomData);
void GetRomCRC2 (DWORD *Crc2, BYTE *RomData);
int GetRomCicChipID (BYTE *RomData);
void RomID (char *ID, BYTE *RomData);
void RomIDPreScanned (char *ID, DWORD *CRC1, DWORD *CRC2, BYTE *Country);

#endif