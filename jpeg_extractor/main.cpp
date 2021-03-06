#include <windows.h>
#include <memory>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>

class HDriveMonicker
{
public:
	HDriveMonicker(HANDLE device)
		: m_device(device)
	{}
	~HDriveMonicker()
	{
		::CloseHandle(m_device);
	}
private:
	HANDLE m_device;
};

void main()
{
	const std::wstring  device_from = L"\\\\.\\PHYSICALDRIVE2";
	const std::wstring  device_to   = L"C:\\";
	const __int64      sector_begin = 0;
	const unsigned int buffer_size  = 5 * 1024 * 1024;

	HANDLE hPhysicalDrive = ::CreateFile(
		device_from.c_str(),
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		0,
		NULL
	);
	if (hPhysicalDrive == INVALID_HANDLE_VALUE)
		return;

	HDriveMonicker device_monicker(hPhysicalDrive);
	DISK_GEOMETRY pdg;
	DWORD junk;
	if (!::DeviceIoControl(hPhysicalDrive, // device to be queried
			IOCTL_DISK_GET_DRIVE_GEOMETRY, // operation to perform
			NULL, 0,                       // no input buffer
			&pdg, sizeof(pdg),             // output buffer
			&junk,                         // # bytes returned
			(LPOVERLAPPED)NULL             // synchronous I/O
		))
	{
		return;
	}

	__int64 hdd_cylinders         = pdg.Cylinders.QuadPart;
	__int64 hdd_tracksPerCylinder = pdg.TracksPerCylinder;
	__int64 hdd_sectorsPerTrack   = pdg.SectorsPerTrack;
	__int64 sectors = hdd_cylinders * hdd_tracksPerCylinder * hdd_sectorsPerTrack;
	__int64 sector  = sector_begin;
	std::wcout << L"DEVICE  = " << device_from << std::endl;
	std::wcout
		<< L"CYL     = " << hdd_cylinders << L", "
		<< L"TRACK = " << hdd_tracksPerCylinder << L", "
		<< L"SECTOR = " << hdd_sectorsPerTrack << L", "
		<< L"SIZE = " << pdg.BytesPerSector <<
	std::endl;
	std::wcout
		<< L"SECTORS = " << sectors << L", "
		<< L"SIZE = " << sectors * pdg.BytesPerSector << L" byte (" << sectors * pdg.BytesPerSector / 1024 / 1024 / 1024 << L"GB)" <<
	std::endl;

	typedef unsigned char uchar;
	std::auto_ptr<uchar> pSector;
	pSector.reset( new uchar[pdg.BytesPerSector] );
	std::auto_ptr<uchar> buffer;
	buffer.reset( new uchar[buffer_size] );

	LARGE_INTEGER li;
	li.QuadPart = sectors * pdg.BytesPerSector;
	::SetFilePointer(hPhysicalDrive, li.LowPart, &li.HighPart, FILE_BEGIN);

	DWORD dw;
	while (true)
	{
		BOOL res = ::ReadFile(hPhysicalDrive, pSector.get(), pdg.BytesPerSector, &dw, NULL);
		if (!res)
		{
			if (sector != sectors)
			{
				sector++;
				LARGE_INTEGER li;
				li.QuadPart = sector * pdg.BytesPerSector;
				::SetFilePointer(hPhysicalDrive, li.LowPart, &li.HighPart, FILE_BEGIN);
				continue;
			}
			else
			{
				break;
			}
		}
		uchar* data = pSector.get();
		if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF)
		{
			std::wstringstream file_name;
			file_name << device_to << L"IMG_" << sector << L".jpg";
			std::ofstream file(file_name.str().c_str(), std::ios_base::out | std::ios_base::binary);
			if (file.good())
			{
				file.write( (char*)data, pdg.BytesPerSector );
				if (::ReadFile(hPhysicalDrive, buffer.get(), buffer_size, &dw, NULL))
				{
					file.write( (char*)buffer.get(), buffer_size );
				}
				std::wcout << L"write " << file_name.str() << std::endl;
				LARGE_INTEGER li;
				li.QuadPart = (sector + 1) * pdg.BytesPerSector;
				::SetFilePointer(hPhysicalDrive, li.LowPart, &li.HighPart, FILE_BEGIN);
			}
			else
			{
				std::wcerr << L"can't create file " << file_name.str() << std::endl;
			}
		}
		sector++;
		if (sector % (sectors / 100) == 0)
		{
			std::wcout << sector / (sectors/100) << L"%" << std::endl;
		}
		if (sector == sectors)
			break;
	}
}
