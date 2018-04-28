/********************************************************************

   wfdrop.c

   Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License.

********************************************************************/

#define INITGUID 
#include <iterator>
#include "winfile.h"
#include "wfdrop.h"
#include "treectl.h"

#include <ole2.h>
#include <shlobj.h>
#include <PathCch.h>
#include <wrl/implements.h>

#ifndef GUID_DEFINED
DEFINE_OLEGUID(IID_IUnknown,            0x00000000L, 0, 0);
DEFINE_OLEGUID(IID_IDropSource,             0x00000121, 0, 0);
DEFINE_OLEGUID(IID_IDropTarget,             0x00000122, 0, 0);
#endif




LPWSTR QuotedDropList(IDataObject *pDataObject)
{
	HDROP hdrop;
	DWORD cFiles, iFile, cchFiles;
	LPWSTR szFiles = nullptr, pch;
	FORMATETC fmtetc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stgmed = {};

	if (pDataObject->GetData(&fmtetc, &stgmed) == S_OK)
	{
		// Yippie! the data is there, so go get it!
		hdrop = reinterpret_cast<HDROP>(stgmed.hGlobal);

		cFiles = DragQueryFileW(hdrop, 0xffffffff, nullptr, 0);
		cchFiles = 0;
		for (iFile = 0; iFile < cFiles; iFile++)
			cchFiles += DragQueryFileW(hdrop, iFile, nullptr, 0) + 1 + 2;

		pch = szFiles = (LPWSTR)LocalAlloc(LMEM_FIXED, cchFiles * sizeof(WCHAR));
		for (iFile = 0; iFile < cFiles; iFile++)
		{
			*pch++ = CHAR_DQUOTE;
			
			auto cchFile = DragQueryFileW(hdrop, iFile, pch, cchFiles);
			pch += cchFile;
			*pch++ = CHAR_DQUOTE;

			if (iFile+1 < cFiles)
				*pch++ = CHAR_SPACE;
			else
				*pch = CHAR_NULL;
				
			cchFiles -= cchFile + 1 + 2;
		}

		// release the data using the COM API
		ReleaseStgMedium(&stgmed);
	}

	return szFiles;
}


HDROP CreateDropFiles(POINT pt, BOOL fNC, LPTSTR pszFiles)
{
    HANDLE hDrop;
    LPBYTE lpList;
    UINT cbList;
	LPTSTR szSrc;

    LPDROPFILES lpdfs;
    TCHAR szFile[MAXPATHLEN];

	cbList = sizeof(DROPFILES) + sizeof(TCHAR);

	szSrc = pszFiles;
    while (szSrc = GetNextFile(szSrc, szFile, COUNTOF(szFile))) 
	{
        QualifyPath(szFile);

		cbList += (wcslen(szFile) + 1)*sizeof(TCHAR);
	}

    hDrop = GlobalAlloc(GMEM_DDESHARE|GMEM_MOVEABLE|GMEM_ZEROINIT, cbList);
    if (!hDrop)
        return nullptr;

    lpdfs = (LPDROPFILES)GlobalLock(hDrop);

    lpdfs->pFiles = sizeof(DROPFILES);
	lpdfs->pt = pt;
	lpdfs->fNC = fNC;
    lpdfs->fWide = TRUE;

	lpList = (LPBYTE)lpdfs + sizeof(DROPFILES);
	szSrc = pszFiles;

    while (szSrc = GetNextFile(szSrc, szFile, COUNTOF(szFile))) {

       QualifyPath(szFile);

       lstrcpy((LPTSTR)lpList, szFile);

       lpList += (wcslen(szFile) + 1)*sizeof(TCHAR);
    }

	GlobalUnlock(hDrop);

	return reinterpret_cast<HDROP>(hDrop);
}

#define BLOCK_SIZE 512

static HRESULT StreamToFile(IStream *stream, TCHAR *szFile)
{
    byte buffer[BLOCK_SIZE];
    DWORD bytes_read;
    DWORD bytes_written;
    HRESULT hr;
	HANDLE hFile;

    hFile = CreateFile( szFile,
          FILE_READ_DATA | FILE_WRITE_DATA,
          FILE_SHARE_READ | FILE_SHARE_WRITE,
          nullptr,
          CREATE_ALWAYS,
          FILE_ATTRIBUTE_TEMPORARY,
          nullptr );

    if (hFile != INVALID_HANDLE_VALUE) {
        do {
            hr = stream->Read(buffer, BLOCK_SIZE, &bytes_read);
			bytes_written = 0;
            if (SUCCEEDED(hr) && bytes_read)
			{
				if (!WriteFile(hFile, buffer, bytes_read, &bytes_written, nullptr))
				{
					hr = HRESULT_FROM_WIN32(GetLastError());
					bytes_written = 0;
				}
			}
        } while (S_OK == hr && bytes_written != 0);
        CloseHandle(hFile);
		if (FAILED(hr))
			DeleteFile(szFile);
		else
			hr = S_OK;
    }
    else
	    hr = HRESULT_FROM_WIN32(GetLastError());

    return hr;
}


LPWSTR QuotedContentList(IDataObject *pDataObject)
{
    FILEGROUPDESCRIPTOR *file_group_descriptor;
    FILEDESCRIPTOR file_descriptor;
	HRESULT hr;
	LPWSTR szFiles = nullptr;

    unsigned short cp_format_descriptor = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
    unsigned short cp_format_contents = RegisterClipboardFormat(CFSTR_FILECONTENTS);

    //Set up format structure for the descriptor and contents
    FORMATETC descriptor_format = {cp_format_descriptor, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
    FORMATETC contents_format = {cp_format_contents, nullptr, DVASPECT_CONTENT, -1, TYMED_ISTREAM};

    // Check for descriptor format type
    hr = pDataObject->QueryGetData(&descriptor_format);
    if (hr == S_OK) 
	{ 
		// Check for contents format type
        hr = pDataObject->QueryGetData(&contents_format);
        if (hr == S_OK)
		{ 
            // Get the descriptor information
            STGMEDIUM sm_desc= {};
      		unsigned int file_index, cchTempPath, cchFiles;
            WCHAR szTempPath[MAX_PATH+1];

            hr = pDataObject->GetData(&descriptor_format, &sm_desc);
			if (hr != S_OK)
				return nullptr;

            file_group_descriptor = (FILEGROUPDESCRIPTOR *) GlobalLock(sm_desc.hGlobal);

			GetTempPath(MAX_PATH, szTempPath);
			cchTempPath = wcslen(szTempPath);

			// calc total size of file names
			cchFiles = 0;
            for (file_index = 0; file_index < file_group_descriptor->cItems; file_index++) 
			{
                file_descriptor = file_group_descriptor->fgd[file_index];
				cchFiles += 1 + cchTempPath + 1 + wcslen(file_descriptor.cFileName) + 2;
			}

			szFiles = (LPWSTR)LocalAlloc(LMEM_FIXED, cchFiles * sizeof(WCHAR));
			szFiles[0] = '\0';

            // For each file, get the name and copy the stream to a file
            for (file_index = 0; file_index < file_group_descriptor->cItems; file_index++)
			{
                file_descriptor = file_group_descriptor->fgd[file_index];
                contents_format.lindex = file_index;
				STGMEDIUM sm_content = {};
                hr = pDataObject->GetData(&contents_format, &sm_content);

                if (hr == S_OK) 
				{
					// Dump stream to a file
					TCHAR szTempFile[MAXPATHLEN*2+1];

					lstrcpy(szTempFile, szTempPath);
					PathCchAppendEx(
						szTempFile,
						std::size(szTempFile),
						file_descriptor.cFileName,
						PATHCCH_ALLOW_LONG_PATHS | PATHCCH_FORCE_ENABLE_LONG_NAME_PROCESS);

					// TODO: make sure all directories between the temp directory and the file have been created
					// paste from zip archives result in file_descriptor.cFileName with intermediate directories

					hr = StreamToFile(sm_content.pstm, szTempFile);

					if (hr == S_OK)
					{
						CheckEsc(szTempFile);

						if (szFiles[0] != '\0')
							lstrcat(szFiles, TEXT(" "));
						lstrcat(szFiles, szTempFile);
					}

					ReleaseStgMedium(&sm_content);
                }
            }

            GlobalUnlock(sm_desc.hGlobal);
            ReleaseStgMedium(&sm_desc);

			if (szFiles[0] == '\0')
			{
				// nothing to copy
				MessageBeep(0);
				LocalFree((HLOCAL)szFiles);	
				szFiles = nullptr;
			}
        }
	}
    return szFiles;
}

//
//	QueryDataObject private helper routine
//
static BOOL QueryDataObject(IDataObject *pDataObject)
{
	FORMATETC fmtetc = { CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    unsigned short cp_format_descriptor = RegisterClipboardFormat(CFSTR_FILEDESCRIPTOR);
    FORMATETC descriptor_format = {0, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
	descriptor_format.cfFormat = cp_format_descriptor;

	// does the data object support CF_HDROP using a HGLOBAL?
	return pDataObject->QueryGetData(&fmtetc) == S_OK || 
			pDataObject->QueryGetData(&descriptor_format) == S_OK;
}

//
//	DropEffect private helper routine
//
static DWORD DropEffect(DWORD grfKeyState, POINTL pt, DWORD dwAllowed)
{
	DWORD dwEffect = 0;

	// 1. check "pt" -> do we allow a drop at the specified coordinates?
	
	// 2. work out that the drop-effect should be based on grfKeyState
	if(grfKeyState & MK_CONTROL)
	{
		dwEffect = dwAllowed & DROPEFFECT_COPY;
	}
	else if(grfKeyState & MK_SHIFT)
	{
		dwEffect = dwAllowed & DROPEFFECT_MOVE;
	}
	
	// 3. no key-modifiers were specified (or drop effect not allowed), so
	//    base the effect on those allowed by the dropsource
	if(dwEffect == 0)
	{
		if(dwAllowed & DROPEFFECT_COPY) dwEffect = DROPEFFECT_COPY;
		if(dwAllowed & DROPEFFECT_MOVE) dwEffect = DROPEFFECT_MOVE;
	}
	
	return dwEffect;
}

namespace {
	namespace wrl = Microsoft::WRL;
	class wf_IdropTargetImpl : public wrl::RuntimeClass<wrl::RuntimeClassFlags<wrl::ClassicCom>, IDropTarget> {
		HWND	m_hWnd;
		IDataObject *m_pDataObject;
		DWORD m_iItemSelected;
		bool  m_fAllowDrop;

		void PaintRectItem(POINTL *ppt)
		{
			HWND hwndLB;
			DWORD iItem;
			POINT pt;
			BOOL fTree;

			// could be either tree control or directory list box
			hwndLB = GetDlgItem(this->m_hWnd, IDCW_LISTBOX);
			fTree = FALSE;
			if (hwndLB == nullptr)
			{
				hwndLB = GetDlgItem(this->m_hWnd, IDCW_TREELISTBOX);
				fTree = TRUE;

				if (hwndLB == nullptr)
					return;
			}

			if (ppt != nullptr)
			{
				pt.x = ppt->x;
				pt.y = ppt->y;
				ScreenToClient(hwndLB, &pt);

				iItem = SendMessage(hwndLB, LB_ITEMFROMPOINT, 0, MAKELPARAM(pt.x, pt.y));
				iItem &= 0xffff;
				if (this->m_iItemSelected != -1 && this->m_iItemSelected == iItem)
					return;
			}

			// unpaint old item
			if (this->m_iItemSelected != -1)
			{
				if (fTree)
					RectTreeItem(hwndLB, this->m_iItemSelected, FALSE);
				else
					DSRectItem(hwndLB, this->m_iItemSelected, FALSE, FALSE);

				this->m_iItemSelected = (DWORD)-1;
			}

			// if new item, paint it.
			if (ppt != nullptr)
			{
				if (fTree)
				{
					if (RectTreeItem(hwndLB, iItem, TRUE))
						this->m_iItemSelected = iItem;
				}
				else
				{
					if (DSRectItem(hwndLB, iItem, TRUE, FALSE))
						this->m_iItemSelected = iItem;
				}
			}
		}

		void DropData(IDataObject *pDataObject, DWORD dwEffect)
		{
			// construct a FORMATETC object
			HWND hwndLB;
			BOOL fTree;
			LPWSTR szFiles = nullptr;
			WCHAR     szDest[MAXPATHLEN];

			hwndLB = GetDlgItem(this->m_hWnd, IDCW_LISTBOX);
			fTree = FALSE;
			if (hwndLB == nullptr)
			{
				hwndLB = GetDlgItem(this->m_hWnd, IDCW_TREELISTBOX);
				fTree = TRUE;

				if (hwndLB == nullptr)
					return;
			}

			// if item selected, add path
			if (fTree)
			{
				PDNODE pNode;

				// odd
				if (this->m_iItemSelected == -1)
					return;

				if (SendMessageW(hwndLB, LB_GETTEXT, this->m_iItemSelected, (LPARAM)&pNode) == LB_ERR)
					return;

				GetTreePath(pNode, szDest);
			}
			else
			{
				LPXDTA    lpxdta;

				SendMessageW(this->m_hWnd, FS_GETDIRECTORY, COUNTOF(szDest), (LPARAM)szDest);

				if (this->m_iItemSelected != -1)
				{
					SendMessageW(hwndLB, LB_GETTEXT, this->m_iItemSelected,
						(LPARAM)(LPTSTR)&lpxdta);

					AddBackslash(szDest);
					lstrcat(szDest, MemGetFileName(lpxdta));
				}
			}

			AddBackslash(szDest);
			lstrcat(szDest, szStarDotStar);   // put files in this dir

			CheckEsc(szDest);

			// See if the dataobject contains any TEXT stored as a HGLOBAL
			if ((szFiles = QuotedDropList(pDataObject)) == nullptr)
			{
				szFiles = QuotedContentList(pDataObject);
				dwEffect = DROPEFFECT_MOVE;
			}

			if (szFiles != nullptr)
			{
				SetFocus(this->m_hWnd);

				DMMoveCopyHelper(szFiles, szDest, dwEffect == DROPEFFECT_COPY);

				LocalFree((HLOCAL)szFiles);
			}
		}

	public:

		wf_IdropTargetImpl(HWND hwnd)
			:m_hWnd(hwnd),
			m_pDataObject(nullptr),
			m_iItemSelected(0),
			m_fAllowDrop(false)
		{
		}
		 
		COM_DECLSPEC_NOTHROW STDMETHODIMP DragEnter(
			/* [unique][in] */ __RPC__in_opt IDataObject *pDataObj,
			/* [in] */ DWORD grfKeyState,
			/* [in] */ POINTL pt,
			/* [out][in] */ __RPC__inout DWORD *pdwEffect) noexcept override final
		{
			// does the dataobject contain data we want?
			this->m_fAllowDrop = QueryDataObject(pDataObj);

			if (this->m_fAllowDrop)
			{
				// get the dropeffect based on keyboard state
				*pdwEffect = DropEffect(grfKeyState, pt, *pdwEffect);

				SetFocus(this->m_hWnd);

				this->PaintRectItem(&pt);
			}
			else
			{
				*pdwEffect = DROPEFFECT_NONE;
			}

			return S_OK;
		}

		COM_DECLSPEC_NOTHROW STDMETHODIMP DragOver(
			/* [in] */ DWORD grfKeyState,
			/* [in] */ POINTL pt,
			/* [out][in] */ __RPC__inout DWORD *pdwEffect) noexcept override final {
			if (this->m_fAllowDrop)
			{
				*pdwEffect = DropEffect(grfKeyState, pt, *pdwEffect);
				this->PaintRectItem(&pt);
			}
			else
			{
				*pdwEffect = DROPEFFECT_NONE;
			}

			return S_OK;
		}

		COM_DECLSPEC_NOTHROW STDMETHODIMP DragLeave(void) noexcept override final {
			this->PaintRectItem(nullptr);
			return S_OK;
		}

		COM_DECLSPEC_NOTHROW STDMETHODIMP Drop(
			/* [unique][in] */ __RPC__in_opt IDataObject *pDataObj,
			/* [in] */ DWORD grfKeyState,
			/* [in] */ POINTL pt,
			/* [out][in] */ __RPC__inout DWORD *pdwEffect) noexcept override final {

			if (this->m_fAllowDrop)
			{
				*pdwEffect = DropEffect(grfKeyState, pt, *pdwEffect);

				this->DropData(pDataObj, *pdwEffect);
			}
			else
			{
				*pdwEffect = DROPEFFECT_NONE;
			}

			return S_OK;

		}

	};
}


void RegisterDropWindow(HWND hwnd, IDropTarget **ppDropTarget)
{
	
	auto target = wrl::Make<wf_IdropTargetImpl>(hwnd);

	// acquire a strong lock
	CoLockObjectExternal(target.Get(), TRUE, FALSE);

	// tell OLE that the window is a drop target
	RegisterDragDrop(hwnd, target.Get());
	
	*ppDropTarget = target.Detach();;
}

void UnregisterDropWindow(HWND hwnd, IDropTarget *pDropTarget)
{
	wrl::ComPtr<IDropTarget> finalTarget;
	finalTarget.Attach(pDropTarget);
	// remove drag+drop
	RevokeDragDrop(hwnd);

	// remove the strong lock
	CoLockObjectExternal(finalTarget.Get(), FALSE, TRUE);
}