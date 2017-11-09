#include <Windows.h>
#include <gdiplus.h>
#include <Commdlg.h>
#include <Commctrl.h>
#include "Filtros.h"
#include "resource.h"
#include "OpenFileDialog.h"
#include "List.h"

using namespace imageProcesing;

HINSTANCE hInst;
HDC device;
HBITMAP g_imagen = NULL;
List<Bitmap*> *modifiedBitmaps;

HACCEL htable;

BYTE g_threshold;
float g_contrast;
float g_pow;
float g_alpha;

int actualId;
bool imagenBlancoYNegro[' '];

Bitmap *resize(Bitmap *bitmap, int width, int height)
{
	int oldWidth = bitmap->GetWidth();
	int oldHeight = bitmap->GetHeight();
	int newWidth = width;
	int newHeight = height;

	double ratio = ((double)oldWidth / (double)oldHeight);
	if(oldWidth > oldHeight)
		newHeight = ((double)newWidth / ratio);
	else
		newWidth = ((double)newHeight * ratio);

	Bitmap *nbmp = new Bitmap(newWidth, newHeight, bitmap->GetPixelFormat());
	Graphics graphics(nbmp);
	graphics.DrawImage(bitmap, 0, 0, newWidth, newHeight);
	return nbmp;
}

bool DrawBitmap(HBITMAP hbitmap, HDC hWinDC, int x, int y)
{
	if(hbitmap == NULL)
		return false;

	HDC hLocalIDC;
	hLocalIDC = CreateCompatibleDC(hWinDC);
	if(hLocalIDC == NULL)
		return false;

	BITMAP qbitmap;
	int iReturn = GetObject(reinterpret_cast<HGDIOBJ>(hbitmap), sizeof(BITMAP), reinterpret_cast<LPVOID>(&qbitmap));
	if(!iReturn)
		return false;

	HBITMAP holdbitmap= (HBITMAP) SelectObject(hLocalIDC, hbitmap);
	if(holdbitmap == NULL)
		return false;

	BOOL qRetBlit = BitBlt(hWinDC, x, y, qbitmap.bmWidth, qbitmap.bmHeight, hLocalIDC, 0, 0, SRCCOPY);
	if(!qRetBlit)
		return false;

	SelectObject(hLocalIDC, holdbitmap);
	DeleteDC(hLocalIDC);
	DeleteObject(hbitmap);
	return true;
}

void ClearScreen()
{
	Bitmap *screen = new Bitmap(800, 600, PixelFormat24bppRGB);
	Graphics g(screen);
	SolidBrush *pen = new SolidBrush(Color::White);
	g.FillRectangle(pen,0,0,800,600);
	HBITMAP hbmp;
	screen->GetHBITMAP(Color(255,255,255), &hbmp);
	DrawBitmap(hbmp, device, 0, 0);
	delete screen;
	delete pen;
}

void RegistrarClaseVentana(const TCHAR *NombreClaseVentana, WNDPROC WndProc) 
{
    WNDCLASSEX WinClass;
    WinClass.cbSize        = sizeof(WNDCLASSEX);
    WinClass.style         = 0;
    WinClass.lpfnWndProc   = WndProc;
    WinClass.cbClsExtra    = 0;
    WinClass.cbWndExtra    = 0;
	WinClass.hInstance     = hInst;
    WinClass.hIcon         = NULL;
    WinClass.hIconSm       = NULL;
    WinClass.hCursor       = LoadCursor(0, IDC_ARROW);
    WinClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    WinClass.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU1);
    WinClass.lpszClassName = NombreClaseVentana;
    ATOM A = RegisterClassEx(&WinClass);
}

static LRESULT CALLBACK DialogProcedure( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam ) 
{
	switch(Msg)
	{
	case WM_INITDIALOG:
		{
			SendMessage(GetDlgItem(hWnd, IDC_TRESH_SLIDER), TBM_SETSEL, (WPARAM) FALSE, (LPARAM) MAKELONG(0, 255)); 
			SendMessage(GetDlgItem(hWnd, IDC_TRESH_SLIDER), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(0, 255));

			SendMessage(GetDlgItem(hWnd, IDC_CONTRAST_SLIDER), TBM_SETSEL, (WPARAM) FALSE, (LPARAM) MAKELONG(0, 255)); 
			SendMessage(GetDlgItem(hWnd, IDC_CONTRAST_SLIDER), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(0, 255));

			SendMessage(GetDlgItem(hWnd, IDC_POW_SLIDER), TBM_SETSEL, (WPARAM) FALSE, (LPARAM) MAKELONG(10, 150)); 
			SendMessage(GetDlgItem(hWnd, IDC_POW_SLIDER), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(10, 150));

			SendMessage(GetDlgItem(hWnd, IDC_ALPHA_SLIDER), TBM_SETSEL, (WPARAM) FALSE, (LPARAM) MAKELONG(10, 190)); 
			SendMessage(GetDlgItem(hWnd, IDC_ALPHA_SLIDER), TBM_SETRANGE, (WPARAM) TRUE, (LPARAM) MAKELONG(10, 190));
			
			return TRUE;
		}

	case WM_COMMAND:
		switch(wParam)
		{
		case IDOK:
			{
				g_threshold = ((float)SendMessage(GetDlgItem(hWnd, IDC_TRESH_SLIDER), TBM_GETPOS, 0, 0));
				g_contrast = ((float)SendMessage(GetDlgItem(hWnd, IDC_CONTRAST_SLIDER), TBM_GETPOS, 0, 0));
				g_pow = ((float)SendMessage(GetDlgItem(hWnd, IDC_POW_SLIDER), TBM_GETPOS, 0, 0) / 100);
				g_alpha = ((float)SendMessage(GetDlgItem(hWnd, IDC_ALPHA_SLIDER), TBM_GETPOS, 0, 0) / 100);
				EndDialog(hWnd, 0);
				return TRUE;
			}
		case IDCANCEL:
			{
				EndDialog(hWnd, 0);
				return TRUE;
			}
		case IDCLOSE:
			{
				EndDialog(hWnd, 0);
				return TRUE;
			}
		}
		break;
	}

	return FALSE;
}

static LRESULT CALLBACK DialogHistograma( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam ) 
{
	switch(Msg)
	{
	case WM_INITDIALOG:
		{			
			return TRUE;
		}
	case WM_PAINT:
		{
			ULONG_PTR gdiplusToken;
			GdiplusStartupInput gdiplusStartupInput;
			GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

			HBITMAP hbmp;
			histograma *histo = new histograma(modifiedBitmaps->get(actualId));
			if(imagenBlancoYNegro[actualId])
				histo->getHistoBitmap(histograma::modo::barras)->GetHBITMAP(Color(255,255,255), &hbmp);
			else
				histo->getHistoBitmap(histograma::modo::lineas)->GetHBITMAP(Color(255,255,255), &hbmp);
			DrawBitmap(hbmp, GetDC(hWnd), 10, 10);

			GdiplusShutdown(gdiplusToken);
			break;
		}
	case WM_COMMAND:
		switch(wParam)
		{
		case IDCLOSE:
			{
				EndDialog(hWnd, 0);
				return TRUE;
			}
		case IDC_OK:
			{
				EndDialog(hWnd, 0);
				return TRUE;
			}
		}
		break;
	}

	return FALSE;
}

static LRESULT CALLBACK WindowProcedure( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam ) 
{
	HDC hdc;
	HDC hdcMem;
	PAINTSTRUCT ps;

    switch (Msg) 
	{ 
	case WM_CREATE:
		{
			break;
		}
	case WM_PAINT:
		{
			ClearScreen();
			if(modifiedBitmaps->getSize() > 0)
			{
				resize(modifiedBitmaps->getLast(), 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
				DrawBitmap(g_imagen, device, 10, 10);
			}
			break;
		}
	case WM_COMMAND:
		{
			int id = LOWORD(wParam);
			switch(id)
			{
			case ID_ACCELERATOR_BACK:
				{
					if(modifiedBitmaps->getSize() > 0 && actualId > 0)
					{
						actualId--;
						Bitmap * bitmap =  modifiedBitmaps->get(actualId);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10);
					}
					break;
				}
			case ID_ACCELERATOR_REDO:
				{
					if(actualId < modifiedBitmaps->getSize() - 1)
					{
						actualId++;
						Bitmap * bitmap =  modifiedBitmaps->get(actualId);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10);
					}
					break;
				}
			}
			switch(LOWORD(wParam))
			{
			case ID_ARCHIVO_SALIR:
				{
					PostQuitMessage(0);
					return 0;
					break;
				}
			case ID_ARCHIVO_ABRIR:
				{
					OpenFileDialog* openFileDialog1 = new OpenFileDialog();

					if (openFileDialog1->ShowDialog())
					{
						Bitmap * bitmap = new Bitmap(openFileDialog1->getFileName());
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						ClearScreen();
						DrawBitmap(g_imagen, device, 10, 10);
						if(modifiedBitmaps->getSize() > 0)
							modifiedBitmaps->clear();
						modifiedBitmaps->add(bitmap);
						actualId = modifiedBitmaps->getSize() - 1;
						imagenBlancoYNegro[actualId] = false;
					}

					delete openFileDialog1;
					break;
				}
			case ID_ARCHIVO_GUARDAR:
				{
					break;
				}
			case ID_FILTERS_LUMINANCIA:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					Filtros::bwLuminancy(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = true;
					break;
				}
			case ID_FILTERS_LUMINOSIDAD:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					Filtros::bwLuminacy(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = true;
					break;
				}
			case ID_FILTERS_PROMEDIO:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					Filtros::bwAverage(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = true;
					break;
				}
			case ID_FILTERS_SEPIA:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					Filtros::sepia(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTERS_BINARIO:
				{
					if (imagenBlancoYNegro)
					{
						DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, reinterpret_cast<DLGPROC>(DialogProcedure));
						Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
						bitmap =  Filtros::binary(bitmap, g_threshold);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10); 
						modifiedBitmaps->add(bitmap);
						
						actualId = modifiedBitmaps->getSize() - 1;
						imagenBlancoYNegro[actualId] = true;
					}
					else
						MessageBox(hWnd, (LPCWSTR)L"favor de convertir la imagen a blanco y negro", (LPCWSTR)L"Atencion", MB_ICONWARNING | MB_OK);
					break;
				}
			case ID_EFECTOS_GRASSFIRE:
				{
					if (imagenBlancoYNegro)
					{
						DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, reinterpret_cast<DLGPROC>(DialogProcedure));
						Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
						bitmap =  Filtros::grassfire(bitmap, g_threshold);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10); 
						modifiedBitmaps->add(bitmap);
						
						actualId = modifiedBitmaps->getSize() - 1;
						imagenBlancoYNegro[actualId] = true;
					}
					else
						MessageBox(hWnd, (LPCWSTR)L"favor de convertir la imagen a blanco y negro", (LPCWSTR)L"Atencion", MB_ICONWARNING | MB_OK);
					break;
				}
			case ID_EFECTOS_ESQUELETO:
				{
					if (imagenBlancoYNegro)
					{
						DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, reinterpret_cast<DLGPROC>(DialogProcedure));
						Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
						bitmap =  Filtros::topologicalSkeleton(bitmap, g_threshold);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10); 
						modifiedBitmaps->add(bitmap);
						
						actualId = modifiedBitmaps->getSize() - 1;
						imagenBlancoYNegro[actualId] = true;
					}
					else
						MessageBox(hWnd, (LPCWSTR)L"favor de convertir la imagen a blanco y negro", (LPCWSTR)L"Atencion", MB_ICONWARNING | MB_OK);
					break;
				}
			case ID_EFECTOS_NEGATIVO:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::negativo(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_DIRECCIONALES_NORTE:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::norte(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_DIRECCIONALES_SUR:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::sur(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_DIRECCIONALES_ESTE:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::este(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_DIRECCIONALES_OESTE:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::oeste(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_CORRECCIONLOGARITMICA:
				{
					if (imagenBlancoYNegro)
					{
						DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, reinterpret_cast<DLGPROC>(DialogProcedure));
						Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
						bitmap =  Filtros::correccionLogaritmica(bitmap, g_contrast);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10); 
						modifiedBitmaps->add(bitmap);
						
						actualId = modifiedBitmaps->getSize() - 1;
						imagenBlancoYNegro[actualId] = false;
					}
					else
						MessageBox(hWnd, (LPCWSTR)L"favor de convertir la imagen a blanco y negro", (LPCWSTR)L"Atencion", MB_ICONWARNING | MB_OK);
					break;
				}
			case ID_FILTROS_GAUSSIANO:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::gaussiano(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_SHARPEN:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::sharpen(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_LAPLACIANO:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::laplaciano(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_MEDIA:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::media(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_MEDIANA:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::mediana(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_MEDIAPONDERADA:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::mediaPonderada(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_MENOSLAPLACIANO:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::menosLaplaciano(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_POTENCIA:
				{
					DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, reinterpret_cast<DLGPROC>(DialogProcedure));
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::potencia(bitmap, g_contrast, g_pow);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_RECORTEDEPLANODEBITS:
				{
					DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, reinterpret_cast<DLGPROC>(DialogProcedure));
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::recorteDeBits(bitmap, g_threshold);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_FILTROS_SOBEL:
				{
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					bitmap =  Filtros::sobel(bitmap);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_ECUALIZACION_DESPLAZAMIENTO:
				{
					DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, reinterpret_cast<DLGPROC>(DialogProcedure));
					Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
					Filtros::desplazamiento(bitmap, g_threshold);
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = true;
					break;
				}
			case ID_ECUALIZACION_ECAUALIZAR:
				{
					if (imagenBlancoYNegro)
					{
						Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
						Filtros::ecualizacion(bitmap);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10); 
						modifiedBitmaps->add(bitmap);
						
						actualId = modifiedBitmaps->getSize() - 1;
						imagenBlancoYNegro[actualId] = true;
					}
					else
						MessageBox(hWnd, (LPCWSTR)L"favor de convertir la imagen a blanco y negro", (LPCWSTR)L"Atencion", MB_ICONWARNING | MB_OK);
					break;
				}
			case ID_ECUALIZACION_ECUALIZACIONEXPONENCIAL:
				{
					if (imagenBlancoYNegro)
					{
						Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
						DialogBox(hInst, MAKEINTRESOURCE(IDD_DIALOG1), hWnd, reinterpret_cast<DLGPROC>(DialogProcedure));
						Filtros::ecualizacionExponencial(bitmap, g_alpha);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10); 
						modifiedBitmaps->add(bitmap);
						
						actualId = modifiedBitmaps->getSize() - 1;
						imagenBlancoYNegro[actualId] = true;
					}
					else
						MessageBox(hWnd, (LPCWSTR)L"favor de convertir la imagen a blanco y negro", (LPCWSTR)L"Atencion", MB_ICONWARNING | MB_OK);
					break;
				}
			case ID_ECUALIZACION_ECUALIZACIONSIMPLE:
				{
					if(imagenBlancoYNegro)
					{
						Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
						Filtros::ecualizacionSimple(bitmap);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10); 
						modifiedBitmaps->add(bitmap);
						
						actualId = modifiedBitmaps->getSize() - 1;
						imagenBlancoYNegro[actualId] = true;
					}
					else
						MessageBox(hWnd, (LPCWSTR)L"favor de convertir la imagen a blanco y negro", (LPCWSTR)L"Atencion", MB_ICONWARNING | MB_OK);
					break;
				}
			case ID_ECUALIZACION_ECUALIZACIONUNIFORME:
				{
					if (imagenBlancoYNegro)
					{
						Bitmap * bitmap = modifiedBitmaps->get(actualId)->Clone(0, 0, 
						modifiedBitmaps->getLast()->GetWidth(), 
						modifiedBitmaps->getLast()->GetHeight(), 
						modifiedBitmaps->getLast()->GetPixelFormat());
						Filtros::ecualizacionUniforme(bitmap);
						resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
						DrawBitmap(g_imagen, device, 10, 10); 
						modifiedBitmaps->add(bitmap);
						
						actualId = modifiedBitmaps->getSize() - 1;
						imagenBlancoYNegro[actualId] = true;
					}
					else
						MessageBox(hWnd, (LPCWSTR)L"favor de convertir la imagen a blanco y negro", (LPCWSTR)L"Atencion", MB_ICONWARNING | MB_OK);
					break;
				}
			case ID_RESTAURAR:
				{
					Bitmap * bitmap =  modifiedBitmaps->getFirst()->Clone(0, 0, 
						modifiedBitmaps->getFirst()->GetWidth(), 
						modifiedBitmaps->getFirst()->GetHeight(), 
						modifiedBitmaps->getFirst()->GetPixelFormat());
					resize(bitmap, 760, 520)->GetHBITMAP(Color(255,255,255), &g_imagen);
					DrawBitmap(g_imagen, device, 10, 10);
					modifiedBitmaps->clear();
					modifiedBitmaps->add(bitmap);
					
					actualId = modifiedBitmaps->getSize() - 1;
					imagenBlancoYNegro[actualId] = false;
					break;
				}
			case ID_HISTOGRAMA:
				{
					HWND hDlg = CreateDialogParam(hInst, MAKEINTRESOURCE(IDD_DIALOG2), hWnd, reinterpret_cast<DLGPROC>(DialogHistograma), 0);
					ShowWindow(hDlg, 5);
					break;
				}
			}
			break;
		}
	case WM_CLOSE:
		{
			PostQuitMessage(0);
			return 0;
		}
    }
    return DefWindowProc(hWnd, Msg, wParam, lParam); 
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	hInst = hInstance;
    TCHAR NombreClase[] = TEXT("NombreVentana");
    RegistrarClaseVentana(NombreClase, WindowProcedure);
	HWND hWnd = CreateWindowEx( NULL, NombreClase, TEXT("PIAD"), WS_OVERLAPPEDWINDOW | WS_VISIBLE, 100, 100, 800, 600, NULL, NULL, hInstance, NULL ); 
	device = GetDC(hWnd);

	ULONG_PTR gdiplusToken;
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

	modifiedBitmaps = new List<Bitmap*>();
	htable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

    MSG Mensaje;
    while (TRUE == GetMessage(&Mensaje, NULL, 0, 0)) { 
		if(!TranslateAccelerator(Mensaje.hwnd, htable, &Mensaje))
		{
			TranslateMessage(&Mensaje);                     
			DispatchMessage(&Mensaje);
		}
    } 

	modifiedBitmaps->clear();
	GdiplusShutdown(gdiplusToken);
    return 0;
}