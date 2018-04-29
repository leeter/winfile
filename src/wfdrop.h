/**************************************************************************

   wfdrop.h

   Include for WINFILE program

   Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License.

**************************************************************************/
#pragma once
#ifndef WFDROP_INC
#define WFDROP_INC
#include <ole2.h>

#ifdef __cplusplus
extern "C"{
#endif
void RegisterDropWindow(HWND hwnd, IDropTarget **ppDropTarget);
void UnregisterDropWindow(HWND hwnd, IDropTarget *pDropTarget);



LPWSTR QuotedDropList(IDataObject *pDataObj);
LPWSTR QuotedContentList(IDataObject *pDataObj);
HDROP CreateDropFiles(POINT pt, BOOL fNC, LPTSTR pszFiles);
#ifdef __cplusplus
}
#endif
#endif