#include <Windows.h>
#include <gdiplus.h>
#include <math.h>
#include <algorithm>
#include <unordered_map>

using namespace Gdiplus;
using namespace std;

#define EPSILON 2.7182818
#define PI 3.1416

namespace imageProcesing
{
	class AjustesColor
	{
	public:
		static int saturate(int valor)
		{
			if(valor > 255)
			{
				valor = 255;
			}
			else if(valor < 0)
			{
				valor = 0;
			}
			return valor;
		}
	};

	class histograma: private AjustesColor
	{
	public:
		enum modo {barras, lineas};

	private:
		float *histo[3];
		float *CDF[3];
		unordered_map<BYTE, BYTE> *distribucion;
		int pixelCant;

	public:
		histograma(Bitmap *bitmap)
		{
			histo[0] = new float[256];
			histo[1] = new float[256];
			histo[2] = new float[256];

			CDF[0] = new float[256];
			CDF[1] = new float[256];
			CDF[2] = new float[256];

			distribucion = new unordered_map<BYTE, BYTE>();
			pixelCant = bitmap->GetWidth() * bitmap->GetHeight();

			float *pixels[3];
			pixels[0] = new float[pixelCant];
			pixels[1] = new float[pixelCant];
			pixels[2] = new float[pixelCant];

			for(int i = 0; i < 255; i++)
			{
				histo[0][i] = 0;
				histo[1][i] = 0;
				histo[2][i] = 0;
			}

			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					pixels[0][y * bitmap->GetWidth() + x] = column[x * 3 + 0];
					pixels[1][y * bitmap->GetWidth() + x] = column[x * 3 + 1];
					pixels[2][y * bitmap->GetWidth() + x] = column[x * 3 + 2];
				}
			}

			bitmap->UnlockBits(&data);

			sort(pixels[0], pixels[0] + pixelCant);
			sort(pixels[1], pixels[1] + pixelCant);
			sort(pixels[2], pixels[2] + pixelCant);
			for(int x = 0; x < pixelCant; x++)
			{
				histo[0][(int)pixels[0][x]]++;
				histo[1][(int)pixels[1][x]]++;
				histo[2][(int)pixels[2][x]]++;
			}

			CDF[0][0] = histo[0][0];
			CDF[1][0] = histo[1][0];
			CDF[2][0] = histo[2][0];
			for(int i = 1; i < 256; i++)
			{
				CDF[0][i] = CDF[0][i - 1] + histo[0][i];
				CDF[1][i] = CDF[1][i - 1] + histo[1][i];
				CDF[2][i] = CDF[2][i - 1] + histo[1][i];
			}
		}

		Bitmap* aplicarEcualizacion(Bitmap *bitmap)
		{
			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					BYTE grayscale = saturate(this->getDistribucion()->at(column[x * 3 + 0]));
					column[x * 3 + 0] = grayscale;
					column[x * 3 + 1] = grayscale;
					column[x * 3 + 2] = grayscale;
				}
			}

			bitmap->UnlockBits(&data);
			return bitmap;
		}

		void ecualizar()
		{
			for(int i = 0; i < 256; i++)
			{
				distribucion->emplace(i, saturate(((CDF[0][i] - getCDFMin()) / (pixelCant - getCDFMin())) * 255));
			}
		}

		void ecualizacionSimple()
		{
			for(int i = 0; i < 256; i++)
			{
				distribucion->emplace(i, saturate(255 * (CDF[0][i] / getCDFMaxR())));
			}
		}

		void ecualizacionUniforme()
		{
			for(int i = 0; i < 256; i++)
			{
				distribucion->emplace(i, saturate((getHistoMaxR() - getHistoMin()) * (CDF[0][i] / getCDFMaxR()) + getHistoMin()));
			}
		}

		void ecualizacionExponencial(float alpha)
		{
			for(int i = 0; i < 256; i++)
			{
				distribucion->emplace(i, saturate(getHistoMin() - (1 / alpha) * log(1 - (CDF[0][i] / getCDFMaxR()))));
			}
		}

		void desplazamiento(BYTE des)
		{
			for(int i = 0; i < 256; i++)
			{
				distribucion->emplace(i, saturate(i + des));
			}
		}

		Bitmap *getHistoBitmap(modo mode)
		{
			Bitmap *bitmap = new Bitmap(516, 257, PixelFormat24bppRGB);
			Graphics g(bitmap);

			Pen *pen = new Pen(Color::Black);
			Pen *penR = new Pen(Color::Red);
			Pen *penG = new Pen(Color::Green);
			Pen *penB = new Pen(Color::Blue);

			pen->SetWidth(1);
			penR->SetWidth(1);
			penG->SetWidth(1);
			penB->SetWidth(1);

			g.DrawRectangle(pen,0,0,511,256);
			int maxR = getHistoMaxCountR();
			int maxG = getHistoMaxCountG();
			int maxB = getHistoMaxCountB();
			if(mode == modo::lineas)
			{
				for(int i = 2; i < 256; i++)
				{
					g.DrawLine(penR, Point(i * 2 - 2, 257 - ((histo[0][i - 1] / maxR) * 255)), Point(i * 2, 257 - ((histo[0][i] / maxR) * 255)));
					g.DrawLine(penG, Point(i * 2 - 2, 257 - ((histo[1][i - 1] / maxG) * 255)), Point(i * 2, 257 - ((histo[1][i] / maxG) * 255)));
					g.DrawLine(penB, Point(i * 2 - 2, 257 - ((histo[2][i - 1] / maxB) * 255)), Point(i * 2, 257 - ((histo[2][i] / maxB) * 255)));
				}
			}
			else
			{
				pen->SetColor(Color::White);
				for(int i = 2; i < 256; i++)
				{
					g.DrawLine(pen, Point(i * 2, 257), Point(i * 2, 257 - ((histo[0][i] / maxR) * 255)));
					g.DrawLine(pen, Point(i * 2, 257), Point(i * 2, 257 - ((histo[1][i] / maxG) * 255)));
					g.DrawLine(pen, Point(i * 2, 257), Point(i * 2, 257 - ((histo[2][i] / maxB) * 255)));
				}
			}

			return bitmap;
		}

		int getHistoMaxCountR()
		{
			float *arr = new float[256];
			for (int i = 0; i < 256; i++)
				arr[i] = histo[0][i];
			sort(arr, arr + 255);
			int i = 255;
			while(arr[i] <= 0)
				i--;
			return arr[i];
		}

		int getHistoMaxCountG()
		{
			float *arr = new float[256];
			for (int i = 0; i < 256; i++)
				arr[i] = histo[1][i];
			sort(arr, arr + 255);
			int i = 255;
			while(arr[i] <= 0)
				i--;
			return arr[i];
		}

		int getHistoMaxCountB()
		{
			float *arr = new float[256];
			for (int i = 0; i < 256; i++)
				arr[i] = histo[2][i];
			sort(arr, arr + 255);
			int i = 255;
			while(arr[i] <= 0)
				i--;
			return arr[i];
		}

		float getHistoMin()
		{
			int i = 0;
			while(histo[0][i] <= 0)
				i++;
			return i;
		}

		float getHistoMaxR()
		{
			int i = 255;
			while(histo[0][i] <= 0)
				i--;
			return i;
		}

		float getHistoMaxG()
		{
			int i = 255;
			while(histo[1][i] <= 0)
				i--;
			return i;
		}

		float getHistoMaxB()
		{
			int i = 255;
			while(histo[2][i] <= 0)
				i--;
			return i;
		}

		float getCDFMin()
		{
			int i = 0;
			while(CDF[0][i] <= 0)
				i++;
			return CDF[0][i];
		}

		float getCDFMaxR()
		{
			int i = 255;
			while(CDF[0][i] <= 0)
				i--;
			return CDF[0][i];
		}

		float getCDFMaxG()
		{
			int i = 255;
			while(CDF[1][i] <= 0)
				i--;
			return CDF[1][i];
		}

		float getCDFMaxB()
		{
			int i = 255;
			while(CDF[2][i] <= 0)
				i--;
			return CDF[2][i];
		}

		unordered_map<BYTE, BYTE> *getDistribucion()
		{
			return distribucion;
		}
	};

	class Mascara: private AjustesColor
	{
	private:
		float *mask;
		int tamaño;

	public:
		Mascara(int tamaño)
		{
			this->tamaño = tamaño;
			mask = new float[tamaño * tamaño];
		}

		Mascara(int tamaño, float *mascara)
		{
			this->tamaño = tamaño;
			mask = mascara;
		}

		Bitmap *aplyMaskOnBitmap(Bitmap *bitmap)
		{
			Bitmap *resultado = bitmap->Clone(0, 0, bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetPixelFormat());

			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead, bitmap->GetPixelFormat(), &data);

			BitmapData newdata;
			resultado->LockBits(
				new Rect(0, 0, resultado->GetWidth(), resultado->GetHeight()),
				ImageLockMode::ImageLockModeWrite, resultado->GetPixelFormat(), &newdata);

			float sumatoria = getSigma();
			float limite = ((tamaño - 1) / 2);

			for(int y = limite; y < (bitmap->GetHeight() - limite); y++)
			{
				BYTE *oldColumn = (BYTE*) data.Scan0 + (y * data.Stride);
				BYTE *newColumn = (BYTE*) newdata.Scan0 + (y * newdata.Stride);

				for(int x = limite; x < (bitmap->GetWidth() - limite); x++)
				{
					newColumn[x * 3 + 0] = saturate(abs(multiply(getBitmapArea(data, x, y, tamaño, 1)).getSigma() / sumatoria));
					newColumn[x * 3 + 1] = saturate(abs(multiply(getBitmapArea(data, x, y, tamaño, 2)).getSigma() / sumatoria));
					newColumn[x * 3 + 2] = saturate(abs(multiply(getBitmapArea(data, x, y, tamaño, 3)).getSigma() / sumatoria));
				}
			}

			bitmap->UnlockBits(&data);
			resultado->UnlockBits(&newdata);

			return resultado;
		}

		static Mascara getBitmapArea(BitmapData data, int px, int py, int tamaño, int canal)
		{
			Mascara *m = new Mascara(tamaño);
			int limite = ((tamaño - 1) / 2);

			for(int y =  py - limite; y <= (py + limite); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = px - limite; x <= (px + limite); x++)
				{
					switch(canal)
					{
					case 1:
						{
							m->setValue(x - px, y - py, column[x * 3 + 0]);
							break;
						}
					case 2:
						{
							m->setValue(x - px, y - py, column[x * 3 + 1]);
							break;
						}
					case 3:
						{
							m->setValue(x - px, y - py, column[x * 3 + 2]);
							break;
						}
					}
				}
			}
			
			return *m;
		}

		Mascara multiply(Mascara m)
		{
			Mascara *resul = new Mascara(tamaño);
			int limite = ((tamaño - 1) / 2);
			for(int x = -limite; x <= limite; x++)
			{
				for(int y = -limite; y <= limite; y++)
				{
					resul->setValue(x, y, getValueof(x, y) * m.getValueof(x, y));
				}
			}
			return *resul;
		}

		void setValue(int x, int y, float value)
		{
			if(x == -1)
				x = 0;
			else if(x == 0)
				x = 1;
			else if(x == 1)
				x = 2;

			if(y == -1)
				y = 0;
			else if(y == 0)
				y = 1;
			else if(y == 1)
				y = 2;

			mask[y * tamaño + x] = value;
		}

		void fillMask(float value)
		{
			float limite = ((tamaño - 1) / 2);
			for(int x = -limite; x <= limite; x++)
			{
				for(int y = -limite; y <= limite; y++)
				{
					setValue(x,y,value);
				}
			}
		}

		float getValueof(int x, int y)
		{
			if(x == -1)
				x = 0;
			else if(x == 0)
				x = 1;
			else if(x == 1)
				x = 2;

			if(y == -1)
				y = 0;
			else if(y == 0)
				y = 1;
			else if(y == 1)
				y = 2;

			float resul = mask[y * tamaño + x];
			return resul;
		}

		float getSigma()
		{
			float sumatoria = 0;
			for(int x = 0; x < tamaño; x++)
			{
				for(int y = 0; y < tamaño; y++)
				{
					sumatoria += mask[y * tamaño + x];
				}
			}
			if(sumatoria > 0)
				return sumatoria;
			else 
				return 1;
		}

		int getSize()
		{
			return tamaño * tamaño;
		}

		int getMidValue()
		{
			float *m = mask;
			std::sort(m, m + (getSize()));
			return (int)m[((tamaño * tamaño - 1) / 2) + 1];
		}

		int getAncho()
		{
			return tamaño;
		}
	};

	class Filtros: private AjustesColor
	{
	public:
		//Efectos
		static Bitmap* bwLuminancy(Bitmap *bitmap)
		{
			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					int maxRGB = max(column[x * 3 + 0], column[x * 3 + 1]);
					maxRGB = max(maxRGB , column[x * 3 + 2]);
					int minRGB = max(column[x * 3 + 0], column[x * 3 + 1]);
					minRGB = max(minRGB , column[x * 3 + 2]);
					int grayscale = saturate((maxRGB + minRGB) / 2);

					column[x * 3 + 0] = grayscale;
					column[x * 3 + 1] = grayscale;
					column[x * 3 + 2] = grayscale;
				}
			}

			bitmap->UnlockBits(&data);
			return bitmap;
		}

		static Bitmap* bwLuminacy(Bitmap *bitmap)
		{
			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					int grayscale = saturate(column[x * 3 + 2] * 0.3 + column[x * 3 + 1] * 0.59 + column[x * 3 + 0] * 0.11);
					column[x * 3 + 0] = grayscale;
					column[x * 3 + 1] = grayscale;
					column[x * 3 + 2] = grayscale;
				}
			}

			bitmap->UnlockBits(&data);
			return bitmap;
		}

		static Bitmap* bwAverage(Bitmap *bitmap)
		{
			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					int grayscale = saturate((column[x * 3 + 0] + column[x * 3 + 1] + column[x * 3 + 2]) / 3);
					column[x * 3 + 0] = grayscale;
					column[x * 3 + 1] = grayscale;
					column[x * 3 + 2] = grayscale;
				}
			}

			bitmap->UnlockBits(&data);
			return bitmap;
		}

		static Bitmap* sepia(Bitmap *bitmap)
		{
			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					float r = (float)column[x * 3 + 2];
					float g = (float)column[x * 3 + 1];
					float b = (float)column[x * 3 + 0];
					column[x * 3 + 2] = saturate((r * 0.393f) + (g * 0.769f) + (b * 0.189f));
					column[x * 3 + 1] = saturate((r * 0.349f) + (g * 0.686f) + (b * 0.168f));
					column[x * 3 + 0] = saturate((r * 0.272f) + (g * 0.534f) + (b * 0.131f));
				}
			}

			bitmap->UnlockBits(&data);
			return bitmap;
		}

		static Bitmap* negativo(Bitmap *bitmap)
		{
			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					column[x * 3 + 0] = 255 - column[x * 3 + 0];
					column[x * 3 + 1] = 255 - column[x * 3 + 1];
					column[x * 3 + 2] = 255 - column[x * 3 + 2];
				}
			}

			bitmap->UnlockBits(&data);
			return bitmap;
		}

		static Bitmap* binary(Bitmap *bitmap, float threshold)
		{
			Bitmap* resul = bitmap->Clone(0, 0, bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetPixelFormat());

			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead, bitmap->GetPixelFormat(), &data);

			BitmapData datar;
			resul->LockBits(
				new Rect(0, 0, resul->GetWidth(), resul->GetHeight()),
				ImageLockMode::ImageLockModeWrite, resul->GetPixelFormat(), &datar);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				BYTE *columnr = (BYTE*) datar.Scan0 + (y * datar.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					if(column[x * 3 + 0] < threshold)
					{
						columnr[x * 3 + 0] = 0;
						columnr[x * 3 + 1] = 0;
						columnr[x * 3 + 2] = 0;
					}
					else
					{
						columnr[x * 3 + 0] = 255;
						columnr[x * 3 + 1] = 255;
						columnr[x * 3 + 2] = 255;
					}
				}
			}

			resul->UnlockBits(&datar);
			bitmap->UnlockBits(&data);
			return resul;
		}

		static Bitmap* correccionLogaritmica(Bitmap *bitmap, float contraste)
		{
			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					BYTE grayscale = saturate(contraste * log(1.0f + (float)column[x * 3 + 0]));
					column[x * 3 + 0] = grayscale;
					column[x * 3 + 1] = grayscale;
					column[x * 3 + 2] = grayscale;
				}
			}

			bitmap->UnlockBits(&data);
			return bitmap;
		}

		static Bitmap* potencia(Bitmap *bitmap, float contraste, float potencia)
		{
			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					BYTE grayscale = saturate(contraste * pow((float)column[x * 3 + 0], potencia));
					column[x * 3 + 0] = grayscale;
					column[x * 3 + 1] = grayscale;
					column[x * 3 + 2] = grayscale;
				}
			}

			bitmap->UnlockBits(&data);
			return bitmap;
		}

		static Bitmap* recorteDeBits(Bitmap *bitmap, BYTE bits)
		{
			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					column[x * 3 + 0] = saturate((unsigned int)column[x * 3 + 0] & (unsigned int)bits);
					column[x * 3 + 1] = saturate((unsigned int)column[x * 3 + 1] & (unsigned int)bits);
					column[x * 3 + 2] = saturate((unsigned int)column[x * 3 + 2] & (unsigned int)bits);
				}
			}

			bitmap->UnlockBits(&data);
			return bitmap;
		}

		static Bitmap* grassfire(Bitmap *bitmap, float threshold)
		{
			Bitmap* binario = binary(bitmap, threshold);
			Bitmap* resul = new Bitmap(bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetPixelFormat());

			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead|ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			BitmapData dataBinary;
			binario->LockBits(
				new Rect(0, 0, binario->GetWidth(), binario->GetHeight()),
				ImageLockMode::ImageLockModeRead|ImageLockMode::ImageLockModeWrite, binario->GetPixelFormat(), &dataBinary);

			BitmapData datar;
			resul->LockBits(
				new Rect(0, 0, resul->GetWidth(), resul->GetHeight()),
				ImageLockMode::ImageLockModeWrite, resul->GetPixelFormat(), &datar);

			Mascara *mask = new Mascara(3);
			int limite = ((mask->getAncho() - 1) / 2);

			for(int x = limite; x < bitmap->GetWidth() - limite; x++)
			{
				for(int y = bitmap->GetHeight() - (limite * 2); y > limite; y--)
				{
					BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
					BYTE *columnBinary = (BYTE*) dataBinary.Scan0 + (y * dataBinary.Stride);
					BYTE *columnr = (BYTE*) datar.Scan0 + (y * datar.Stride);

					BYTE grayscale;
					if(columnBinary[x * 3 + 0] == 255)
					{
						*mask = Mascara::getBitmapArea(data,x,y,3,1);
						grayscale = 1 + min(mask->getValueof(0,1), mask->getValueof(-1,0));
					}
					else
					{
						grayscale = 0;
					}
					column[x * 3 + 0] = saturate(grayscale);
					column[x * 3 + 1] = saturate(grayscale);
					column[x * 3 + 2] = saturate(grayscale);
				}
			}

			for(int x = bitmap->GetWidth() - (limite * 2); x > limite; x--)
			{
				for(int y = limite; y < bitmap->GetHeight() - limite; y++)
				{
					BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
					BYTE *columnBinary = (BYTE*) dataBinary.Scan0 + (y * dataBinary.Stride);
					BYTE *columnr = (BYTE*) datar.Scan0 + (y * datar.Stride);

					BYTE grayscale;
					if(columnBinary[x * 3 + 0] == 255)
					{
						*mask = Mascara::getBitmapArea(data,x,y,3,1);
						grayscale = min(column[x * 3 + 0], 1 + min(mask->getValueof(0,-1), mask->getValueof(1,0)));
					}
					else
					{
						grayscale = 0;
					}
					column[x * 3 + 0] = saturate(grayscale);
					column[x * 3 + 1] = saturate(grayscale);
					column[x * 3 + 2] = saturate(grayscale);
				}
			}

			bitmap->UnlockBits(&data);
			resul->UnlockBits(&datar);
			binario->UnlockBits(&dataBinary);
			resul = bitmap->Clone(0, 0, bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetPixelFormat());
			return resul;
		}

		static Bitmap* topologicalSkeleton(Bitmap *bitmap, float threshold)
		{
			Bitmap* gf = grassfire(bitmap, threshold);
			histograma* hist = new histograma(gf);

			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			BitmapData datagf;
			gf->LockBits(
				new Rect(0, 0, gf->GetWidth(), gf->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, gf->GetPixelFormat(), &datagf);

			for(int y = 0; y < bitmap->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				BYTE *columngf = (BYTE*) datagf.Scan0 + (y * datagf.Stride);
				for(int x = 0; x < bitmap->GetWidth(); x++)
				{
					BYTE grayscale;
					if(columngf[x * 3 + 0] < threshold)
					{
						grayscale = 0;
					}
					else
					{
						grayscale = 255;
					}
					column[x * 3 + 0] = grayscale;
					column[x * 3 + 1] = grayscale;
					column[x * 3 + 2] = grayscale;
				}
			}

			gf->UnlockBits(&datagf);
			bitmap->UnlockBits(&data);
			return bitmap;
		}

		//Filtros
		static Bitmap* media(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);
			mask->fillMask(1);
			bitmap = mask->aplyMaskOnBitmap(bitmap);

			return bitmap;
		}

		static Bitmap* mediaPonderada(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);
			mask->fillMask(1);
			mask->setValue(0, 0, 2);
			bitmap = mask->aplyMaskOnBitmap(bitmap);

			return bitmap;
		}

		static Bitmap* mediana(Bitmap *bitmap)
		{
			Bitmap *resul = bitmap->Clone(0, 0, bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetPixelFormat());
			Mascara *mask = new Mascara(3);

			int ancho = bitmap->GetWidth();
			int alto = bitmap->GetHeight();
			int max = ((mask->getAncho() - 1) / 2);

			BitmapData data;
			bitmap->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeRead, bitmap->GetPixelFormat(), &data);

			BitmapData datar;
			resul->LockBits(
				new Rect(0, 0, bitmap->GetWidth(), bitmap->GetHeight()),
				ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &datar);

			for(int y = max; y < alto - max; y++)
			{
				BYTE *column = (BYTE*) datar.Scan0 + (y * datar.Stride);
				for(int x = max; x < ancho - max; x++)
				{
					column[x * 3 + 0] = saturate(mask->getBitmapArea(data, x, y, mask->getAncho(), 1).getMidValue());
					column[x * 3 + 1] = saturate(mask->getBitmapArea(data, x, y, mask->getAncho(), 2).getMidValue());
					column[x * 3 + 2] = saturate(mask->getBitmapArea(data, x, y, mask->getAncho(), 3).getMidValue());
				}
			}

			bitmap->UnlockBits(&data);
			resul->UnlockBits(&data);
			return resul;
		}

		static Bitmap* gaussiano(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);

			float corner = pow(EPSILON, (pow(-1, 2) + pow(-1, 2)) / pow(1, 2)) / (2 * PI * pow(1, 2));
			float sides = pow(EPSILON, (pow(0, 2) + pow(-1, 2)) / pow(1, 2)) / (2 * PI * pow(1, 2));
			float center = pow(EPSILON, (pow(0, 2) + pow(0, 2)) / pow(1, 2)) / (2 * PI * pow(1, 2));

			float min = min(corner, sides);
			min = min(min, center);

			corner = corner / min;
			center = center / min;
			sides = sides / min;

			mask->setValue(-1,-1, corner);
			mask->setValue(1,-1, corner);
			mask->setValue(1,1, corner);
			mask->setValue(-1,1, corner);

			mask->setValue(0,-1, sides);
			mask->setValue(0,1, sides);
			mask->setValue(-1,0, sides);
			mask->setValue(1,0, sides);

			mask->setValue(0,0, center);

			bitmap = mask->aplyMaskOnBitmap(bitmap);
			return bitmap;
		}

		static Bitmap* sharpen(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);

			float corner = 0;
			float center = 11;
			float sides = -2;

			mask->setValue(-1,-1, corner);
			mask->setValue(1,-1, corner);
			mask->setValue(1,1, corner);
			mask->setValue(-1,1, corner);

			mask->setValue(0,-1, sides);
			mask->setValue(0,1, sides);
			mask->setValue(-1,0, sides);
			mask->setValue(1,0, sides);

			mask->setValue(0,0, center);

			bitmap = mask->aplyMaskOnBitmap(bitmap);
			return bitmap;
		}

		static Bitmap* laplaciano(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);
			mask->fillMask(0);
			mask->setValue(0, 0, -4);
			mask->setValue(1, 0, 1);
			mask->setValue(-1, 0, 1);
			mask->setValue(0, 1, 1);
			mask->setValue(0, -1, 1);
			bitmap = mask->aplyMaskOnBitmap(bitmap);

			return bitmap;
		}

		static Bitmap* menosLaplaciano(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);
			mask->fillMask(0);
			mask->setValue(0, 0, 5);
			mask->setValue(1, 0, -1);
			mask->setValue(-1, 0, -1);
			mask->setValue(0, 1, -1);
			mask->setValue(0, -1, -1);
			bitmap = mask->aplyMaskOnBitmap(bitmap);

			return bitmap;
		}

		static Bitmap* norte(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);
			mask->setValue(-1, 1, 1);
			mask->setValue(0, 1, 1);
			mask->setValue(1, 1, 1);
			mask->setValue(-1, 0, 1);
			mask->setValue(0, 0, -2);
			mask->setValue(1, 0, 1);
			mask->setValue(-1, -1, -1);
			mask->setValue(0, -1, -1);
			mask->setValue(1, -1, -1);
			bitmap = mask->aplyMaskOnBitmap(bitmap);

			return bitmap;
		}

		static Bitmap* sur(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);
			mask->setValue(-1, 1, -1);
			mask->setValue(0, 1, -1);
			mask->setValue(1, 1, -1);
			mask->setValue(-1, 0, 1);
			mask->setValue(0, 0, -2);
			mask->setValue(1, 0, 1);
			mask->setValue(-1, -1, 1);
			mask->setValue(0, -1, 1);
			mask->setValue(1, -1, 1);
			bitmap = mask->aplyMaskOnBitmap(bitmap);

			return bitmap;
		}

		static Bitmap* este(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);
			mask->setValue(-1, 1, -1);
			mask->setValue(0, 1, 1);
			mask->setValue(1, 1, 1);
			mask->setValue(-1, 0, -1);
			mask->setValue(0, 0, -2);
			mask->setValue(1, 0, 1);
			mask->setValue(-1, -1, -1);
			mask->setValue(0, -1, 1);
			mask->setValue(1, -1, 1);
			bitmap = mask->aplyMaskOnBitmap(bitmap);

			return bitmap;
		}

		static Bitmap* oeste(Bitmap *bitmap)
		{
			Mascara *mask = new Mascara(3);
			mask->setValue(-1, 1, 1);
			mask->setValue(0, 1, 1);
			mask->setValue(1, 1, -1);
			mask->setValue(-1, 0, 1);
			mask->setValue(0, 0, -2);
			mask->setValue(1, 0, -1);
			mask->setValue(-1, -1, 1);
			mask->setValue(0, -1, 1);
			mask->setValue(1, -1, -1);
			bitmap = mask->aplyMaskOnBitmap(bitmap);

			return bitmap;
		}

		static Bitmap* sobel(Bitmap *bitmap)
		{
			Bitmap *bitmapf = bitmap->Clone(0, 0, bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetPixelFormat());
			Bitmap *bitmapc = bitmap->Clone(0, 0, bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetPixelFormat());
			Bitmap *resul = bitmap->Clone(0, 0, bitmap->GetWidth(), bitmap->GetHeight(), bitmap->GetPixelFormat());

			Mascara *maskf = new Mascara(3);
			maskf->setValue(-1, 1, -1);
			maskf->setValue(0, 1, -2);
			maskf->setValue(1, 1, -1);
			maskf->setValue(-1, 0, 0);
			maskf->setValue(0, 0, 0);
			maskf->setValue(1, 0, 0);
			maskf->setValue(-1, -1, 1);
			maskf->setValue(0, -1, 2);
			maskf->setValue(1, -1, 1);
			bitmapf = maskf->aplyMaskOnBitmap(bitmapf);

			Mascara *maskc = new Mascara(3);
			maskc->setValue(-1, 1, -1);
			maskc->setValue(0, 1, 0);
			maskc->setValue(1, 1, 1);
			maskc->setValue(-1, 0, -2);
			maskc->setValue(0, 0, 0);
			maskc->setValue(1, 0, 2);
			maskc->setValue(-1, -1, -1);
			maskc->setValue(0, -1, 0);
			maskc->setValue(1, -1, 1);
			bitmapc = maskc->aplyMaskOnBitmap(bitmapc);

			BitmapData data;
			resul->LockBits(
				new Rect(0, 0, resul->GetWidth(), resul->GetHeight()),
				ImageLockMode::ImageLockModeRead | ImageLockMode::ImageLockModeWrite, bitmap->GetPixelFormat(), &data);

			BitmapData dataf;
			bitmapf->LockBits(
				new Rect(0, 0, bitmapf->GetWidth(), bitmapf->GetHeight()),
				ImageLockMode::ImageLockModeRead, bitmap->GetPixelFormat(), &dataf);

			BitmapData datac;
			bitmapc->LockBits(
				new Rect(0, 0, bitmapc->GetWidth(), bitmapc->GetHeight()),
				ImageLockMode::ImageLockModeRead, bitmap->GetPixelFormat(), &datac);

			for(int y = 0; y < resul->GetHeight(); y++)
			{
				BYTE *column = (BYTE*) data.Scan0 + (y * data.Stride);
				BYTE *columnf = (BYTE*) dataf.Scan0 + (y * dataf.Stride);
				BYTE *columnc = (BYTE*) datac.Scan0 + (y * datac.Stride);
				for(int x = 0; x < resul->GetWidth(); x++)
				{
					column[x * 3 + 0] = saturate(sqrt(pow((float)columnf[x * 3 + 0],2) + pow((float)columnc[x * 3 + 0],2)));
					column[x * 3 + 1] = saturate(sqrt(pow((float)columnf[x * 3 + 1],2) + pow((float)columnc[x * 3 + 1],2)));
					column[x * 3 + 2] = saturate(sqrt(pow((float)columnf[x * 3 + 2],2) + pow((float)columnc[x * 3 + 2],2)));
				}
			}

			resul->UnlockBits(&data);
			bitmapf->UnlockBits(&dataf);
			bitmapc->UnlockBits(&datac);
			return resul;
		}

		//Ecualizaciones
		static Bitmap* ecualizacion(Bitmap *bitmap)
		{
			histograma *histo = new histograma(bitmap);
			histo->ecualizar();
			bitmap = histo->aplicarEcualizacion(bitmap);
			return bitmap;
		}

		static Bitmap* ecualizacionSimple(Bitmap *bitmap)
		{
			histograma *histo = new histograma(bitmap);
			histo->ecualizacionSimple();
			bitmap = histo->aplicarEcualizacion(bitmap);
			return bitmap;
		}

		static Bitmap* ecualizacionUniforme(Bitmap *bitmap)
		{
			histograma *histo = new histograma(bitmap);
			histo->ecualizacionUniforme();
			bitmap = histo->aplicarEcualizacion(bitmap);
			return bitmap;
		}

		static Bitmap* ecualizacionExponencial(Bitmap *bitmap, float alpha)
		{
			histograma *histo = new histograma(bitmap);
			histo->ecualizacionExponencial(alpha);
			bitmap = histo->aplicarEcualizacion(bitmap);
			return bitmap;
		}

		static Bitmap* desplazamiento(Bitmap *bitmap, float des)
		{
			histograma *histo = new histograma(bitmap);
			histo->desplazamiento(des);
			bitmap = histo->aplicarEcualizacion(bitmap);
			return bitmap;
		}
	};
};