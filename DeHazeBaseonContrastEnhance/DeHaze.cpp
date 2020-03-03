#include "DeHaze.h"

void DeHazeBaseonContrastEnhance(cv::Mat& srcImg, cv::Mat& dstImg, cv::Size& transBlockSize, float fLambda, int guidedRadius, double eps, float fGamma /*= 1*/)
{
	try
	{
		if (srcImg.data == nullptr || srcImg.empty() || transBlockSize.width <= 0 || transBlockSize.height <= 0 || fLambda <= 0 || guidedRadius <= 0)
		{
			throw "error:Input params error.";
		}
		std::vector<float> vAtom;
		cv::Mat transmission;
		if (srcImg.channels() == 3)
		{
			vAtom.push_back(255);
			vAtom.push_back(255);
			vAtom.push_back(255);
		}
		else
		{
			vAtom.push_back(255);
		}
		double exec_time = (double)cv::getTickCount();
		EstimateAirlight(srcImg, cv::Size(20, 20), vAtom);
		exec_time = ((double)cv::getTickCount() - exec_time)*1000. / cv::getTickFrequency();
		std::cout << exec_time / 1000.0 << std::endl;
		exec_time = (double)cv::getTickCount();
		EstimateTransmission(srcImg, transmission, transBlockSize, fLambda, vAtom);
		exec_time = ((double)cv::getTickCount() - exec_time)*1000. / cv::getTickFrequency();
		std::cout << exec_time / 1000.0 << std::endl;
		exec_time = (double)cv::getTickCount();
		RefiningTransmission(transmission, srcImg, transmission, guidedRadius, eps);
		exec_time = ((double)cv::getTickCount() - exec_time)*1000. / cv::getTickFrequency();
		std::cout << exec_time / 1000.0 << std::endl;
		exec_time = (double)cv::getTickCount();
		RestoreImage(srcImg, transmission, dstImg, vAtom);
		exec_time = ((double)cv::getTickCount() - exec_time)*1000. / cv::getTickFrequency();
		std::cout << exec_time / 1000.0 << std::endl;

		GammaTransform(dstImg, dstImg, fGamma);
	}
	catch (cv::Exception& e)
	{
		throw e;
	}
	catch (std::exception& e)
	{
		throw e;
	}
}

void EstimateAirlight(cv::Mat& srcImage, cv::Size& minSize, std::vector<float>& vAtom)
{
	try
	{
		if (minSize.height <= 0 || minSize.width <= 0)
		{
			throw "params error";
		}
		if ((srcImage.channels() == 3 && vAtom.size() != 3) || (srcImage.channels() != 3 && vAtom.size() == 3))
		{
			throw "params error";
		}
		cv::Mat holeImage = srcImage;
		int width = holeImage.cols;
		int height = holeImage.rows;
		while (width*height > minSize.height*minSize.width)
		{
			std::vector<cv::Mat> fourSection;
			cv::Mat ulImage = holeImage(cv::Rect(0, 0, int(width / 2), int(height / 2)));
			cv::Mat urImage = holeImage(cv::Rect(int(width / 2), 0, width - int(width / 2), int(height / 2)));
			cv::Mat brImage = holeImage(cv::Rect(int(width / 2), int(height / 2), width - int(width / 2), height - int(height / 2)));
			cv::Mat blImage = holeImage(cv::Rect(0, int(height / 2), int(width / 2), height - int(height / 2)));
			fourSection.push_back(ulImage);
			fourSection.push_back(urImage);
			fourSection.push_back(brImage);
			fourSection.push_back(blImage);
			double maxScore = 0;
			double score = 0;

			cv::Mat tempMat;
			for (int i = 0; i < 4; i++)
			{

				cv::Mat meanMat, stdMat;
				cv::meanStdDev(fourSection[i], meanMat, stdMat);
				//��Ϊ3ͨ���͵�ͨ��
				score = fourSection[i].channels() == 3 ? ((meanMat.at<double>(0, 0) - stdMat.at<double>(0, 0)) + (meanMat.at<double>(1, 0) - stdMat.at<double>(1, 0)) + (meanMat.at<double>(2, 0) - stdMat.at<double>(2, 0))) : (meanMat.at<double>(0, 0) - stdMat.at<double>(0, 0));
				if (score > maxScore)
				{
					maxScore = score;
					holeImage = fourSection[i];
					width = holeImage.cols;
					height = holeImage.rows;
				}
			}
		}

		int nDistance = 0;
		int nMinDistance = 65536;
		int nStep = holeImage.step;
		for (int nY = 0; nY < height; nY++)
		{
			for (int nX = 0; nX < width; nX++)
			{
				if (holeImage.channels() == 3)
				{
					nDistance = int(sqrt(float(255 - (uchar)holeImage.data[nY*nStep + nX * 3])*float(255 - (uchar)holeImage.data[nY*nStep + nX * 3])
						+ float(255 - (uchar)holeImage.data[nY*nStep + nX * 3 + 1])*float(255 - (uchar)holeImage.data[nY*nStep + nX * 3 + 1])
						+ float(255 - (uchar)holeImage.data[nY*nStep + nX * 3 + 2])*float(255 - (uchar)holeImage.data[nY*nStep + nX * 3 + 2])));
					if (nMinDistance > nDistance)
					{
						nMinDistance = nDistance;
						vAtom[0] = (uchar)holeImage.data[nY*nStep + nX * 3];
						vAtom[1] = (uchar)holeImage.data[nY*nStep + nX * 3 + 1];
						vAtom[2] = (uchar)holeImage.data[nY*nStep + nX * 3 + 2];

						//���������⣬�������ߴ�������������ڽ���ȥ�����ɫƫ������ĳЩ�����¿�����΢����ȥ��Ч��
						auto smallest = std::min_element(std::begin(vAtom), std::end(vAtom));
						int idxMin = std::distance(std::begin(vAtom), smallest);
						auto largest = std::max_element(std::begin(vAtom), std::end(vAtom));
						int idxMax = std::distance(std::begin(vAtom), largest);
						if (idxMax + idxMin == 1)
						{
							if (vAtom[idxMax] - vAtom[idxMin] > 11)
							{
								vAtom[idxMin] = int((vAtom[idxMax] + vAtom[2]) / 2);
							}
							if (vAtom[idxMax] - vAtom[2] > 11)
							{
								vAtom[2] = int((vAtom[idxMax] + vAtom[idxMin]) / 2);
							}
						}
						else if ((idxMax + idxMin == 2) && (idxMax != idxMin))
						{
							if (vAtom[idxMax] - vAtom[idxMin] > 11)
							{
								vAtom[idxMin] = int((vAtom[idxMax] + vAtom[1]) / 2);
							}
							if (vAtom[idxMax] - vAtom[1] > 11)
							{
								vAtom[1] = int((vAtom[idxMax] + vAtom[idxMin]) / 2);
							}
						}
						else if (idxMax + idxMin == 3)
						{
							if (vAtom[idxMax] - vAtom[idxMin] > 11)
							{
								vAtom[idxMin] = int((vAtom[idxMax] + vAtom[0]) / 2);
							}
							if (vAtom[idxMax] - vAtom[0] > 11)
							{
								vAtom[0] = int((vAtom[idxMax] + vAtom[idxMin]) / 2);
							}
						}
						vAtom[0] = vAtom[0] * 0.95;
						vAtom[1] = vAtom[1] * 0.95;
						vAtom[2] = vAtom[2] * 0.95;
					}
				}
				else
				{
					nDistance = int(sqrt(float(255 - (uchar)holeImage.data[nY*nStep + nX])*float(255 - (uchar)holeImage.data[nY*nStep + nX])));
					if (nMinDistance > nDistance)
					{
						nMinDistance = nDistance;
						vAtom[0] = (uchar)holeImage.data[nY*nStep + nX];
						vAtom[0] = vAtom[0] * 0.95;
					}
				}
			}
		}
	}
	catch (cv::Exception& e)
	{
		throw e;
	}
	catch (std::exception& e)
	{
		throw e;
	}
}

void EstimateTransmission(cv::Mat& srcImage, cv::Mat& transmission, cv::Size& transBlockSize, float costLambda, std::vector<float>& vAtom)
{

	try
	{
		cv::Mat inputImage;
		srcImage.convertTo(inputImage, CV_32S);

		cv::Mat transMat(srcImage.rows, srcImage.cols, srcImage.channels() == 3 ? CV_32SC3 : CV_32SC1, srcImage.channels() == 3 ? cv::Scalar(427, 427, 427) : cv::Scalar(427));
		transmission = cv::Mat(srcImage.rows, srcImage.cols, CV_32FC1, cv::Scalar(0.3));
		cv::Mat atomMat = cv::Mat(srcImage.rows, srcImage.cols, srcImage.channels() == 3 ? CV_32SC3 : CV_32SC1, srcImage.channels() == 3 ? cv::Scalar(vAtom[0], vAtom[1], vAtom[2]) : cv::Scalar(vAtom[0]));

		std::vector<float> vCost;
		std::vector<float> vTrans;

		for (int iterCount = 0; iterCount < 7; iterCount++)
		{
			cv::Mat pilotImage = (inputImage - atomMat) / transMat + atomMat;
			int channels = pilotImage.channels();
			int index = 0;
			for (int startY = 0; startY < inputImage.rows; startY += transBlockSize.height)
			{
				for (int startX = 0; startX < inputImage.cols; startX += transBlockSize.width)
				{
					int endX = __min(startX + transBlockSize.width, inputImage.cols);
					int endY = __min(startY + transBlockSize.height, inputImage.rows);
					double fSumofSLoss = 0;
					double fSumofSquaredOuts = 0;
					double fSumofOuts = 0;
					int nNumofPixels = channels == 3 ? (endY - startY)*(endX - startX) * 3 : (endY - startY)*(endX - startX);

					//cv::Mat roiMat = pilotImage(cv::Rect(startX, startY, endX - startX, endY - startY));				//��ʽ�м���Աȶ�����
					//cv::Mat meanMat, stdMat;																			//��ʽ�м���Աȶ�����
					//cv::meanStdDev(roiMat, meanMat, stdMat);															//��ʽ�м���Աȶ�����

					for (int nY = startY; nY < endY; nY++)
					{
						cv::Vec3i* data = nullptr;
						int* fdata = nullptr;
						if (channels == 3)
							data = pilotImage.ptr<cv::Vec3i>(nY);
						else
							fdata = pilotImage.ptr<int>(nY);
						for (int nX = startX; nX < endX; nX++)
						{
							cv::Vec3i outPut;
							int foutPut;
							if (channels == 3)
							{
								outPut = data[nX];
								if (outPut[0]>255){ fSumofSLoss += (outPut[0] - 255)*(outPut[0] - 255); }
								else if (outPut[0] < 0){ fSumofSLoss += outPut[0] * outPut[0]; }
								if (outPut[1]>255){ fSumofSLoss += (outPut[1] - 255)*(outPut[1] - 255); }
								else if (outPut[1] < 0){ fSumofSLoss += outPut[1] * outPut[1]; }
								if (outPut[2]>255){ fSumofSLoss += (outPut[2] - 255)*(outPut[2] - 255); }
								else if (outPut[2] < 0){ fSumofSLoss += outPut[2] * outPut[2]; }
								fSumofSquaredOuts += outPut[0] * outPut[0] + outPut[1] * outPut[1] + outPut[2] * outPut[2];
								fSumofOuts += outPut[0] + outPut[1] + outPut[2];
								//fSumofSquaredOuts += std::pow((outPut[0] - meanMat.at<double>(0, 0)), 2) + std::pow((outPut[1] - meanMat.at<double>(1, 0)), 2) + std::pow((outPut[2] - meanMat.at<double>(2, 0)), 2); //��ʽ�м���Աȶ�����
							}
							else
							{
								foutPut = fdata[nX];
								if (foutPut > 255){ fSumofSLoss += (foutPut - 255)*(foutPut - 255); }
								else if (foutPut < 0){ fSumofSLoss += foutPut * foutPut; }
								fSumofSquaredOuts += foutPut*foutPut;
								fSumofOuts += foutPut;
								//fSumofSquaredOuts += std::pow((outPut[0] - meanMat.at<double>(0, 0)), 2);							//��ʽ�м���Աȶ�����
							}
						}
					}
					float fMean = fSumofOuts / (float)nNumofPixels;
					float fCost = costLambda*fSumofSLoss / (float)nNumofPixels - (fSumofSquaredOuts / (float)nNumofPixels - fMean*fMean);
					//float fCost = costLambda*fSumofSLoss / (float)nNumofPixels - (fSumofSquaredOuts / (float)nNumofPixels);			//��ʽ�м���Աȶ�����
					if (iterCount == 0)
					{
						vCost.push_back(fCost);
						vTrans.push_back(0.3);
					}
					else if (vCost[index] > fCost)
					{
						vCost[index] = fCost;
						transmission(cv::Rect(startX, startY, endX - startX, endY - startY)) = cv::Scalar(vTrans[index]);
					}
					vTrans[index] += 0.1;
					transMat(cv::Rect(startX, startY, endX - startX, endY - startY)) = channels == 3 ? cv::Scalar(vTrans[index], vTrans[index], vTrans[index]) : cv::Scalar(vTrans[index]);
					index++;
				}
			}
		}
	}
	catch (cv::Exception& e)
	{
		throw e;
	}
}

void RefiningTransmission(cv::Mat& transmission, cv::Mat& srcImage, cv::Mat& refinedTransmission, int r, double eps)
{
	int channels = transmission.channels();
	std::vector<cv::Mat> vInputImage;
	if (channels == 3){ cv::split(transmission, vInputImage); }
	else{ vInputImage.push_back(transmission); }
	channels = srcImage.channels();
	cv::Mat guidedImage;
	if (channels == 3){ cv::cvtColor(srcImage, guidedImage, cv::COLOR_BGR2GRAY); }
	else{ guidedImage = srcImage.clone(); }
	GuidedFileter(vInputImage[0], guidedImage, refinedTransmission, r, eps);
}

void RestoreImage(cv::Mat& srcImage, cv::Mat& transmission, cv::Mat& dstImage, std::vector<float>& vAtom)
{
	cv::Mat inputImage;
	srcImage.convertTo(inputImage, CV_32F);
	cv::Mat trans, transMat;
	transmission.convertTo(trans, CV_32F);
	int srcChannels = srcImage.channels();
	if (srcChannels == 3)
	{
		std::vector<cv::Mat> vTrans;
		vTrans.push_back(trans);
		vTrans.push_back(trans);
		vTrans.push_back(trans);
		cv::merge(vTrans, transMat);
	}
	else
	{
		transMat = trans;
	}
	cv::Mat atomMat = cv::Mat(srcImage.rows, srcImage.cols, srcImage.channels() == 3 ? CV_32FC3 : CV_32FC1, srcImage.channels() == 3 ? cv::Scalar(vAtom[0], vAtom[1], vAtom[2]) : cv::Scalar(vAtom[0]));
	cv::Mat pilotImage = (inputImage - atomMat) / transMat + atomMat;
	pilotImage.convertTo(dstImage, CV_8U);
}

void GuidedFileter(cv::Mat& guidedImage, cv::Mat& inputImage, cv::Mat& outPutImage, int r, double eps)
{
	try
	{
		//ת��Դͼ����Ϣ
		cv::Mat srcImage, srcClone;
		inputImage.convertTo(srcImage, CV_64FC1);
		guidedImage.convertTo(srcClone, CV_64FC1);
		int nRows = srcImage.rows;
		int nCols = srcImage.cols;
		cv::Mat boxResult;
		//����һ�������ֵ
		cv::boxFilter(cv::Mat::ones(nRows, nCols, srcImage.type()),
			boxResult, CV_64FC1, cv::Size(r, r));
		//���ɵ����ֵmean_I
		cv::Mat mean_I;
		cv::boxFilter(srcImage, mean_I, CV_64FC1, cv::Size(r, r));
		//����ԭʼ��ֵmean_p
		cv::Mat mean_p;
		boxFilter(srcClone, mean_p, CV_64FC1, cv::Size(r, r));
		//���ɻ���ؾ�ֵmean_Ip
		cv::Mat mean_Ip;
		cv::boxFilter(srcImage.mul(srcClone), mean_Ip,
			CV_64FC1, cv::Size(r, r));
		cv::Mat cov_Ip = mean_Ip - mean_I.mul(mean_p);
		//��������ؾ�ֵmean_II
		cv::Mat mean_II;
		//Ӧ�ú��˲���������ص�ֵ
		cv::boxFilter(srcImage.mul(srcImage), mean_II,
			CV_64FC1, cv::Size(r, r));
		//��������������ϵ��
		cv::Mat var_I = mean_II - mean_I.mul(mean_I);
		cv::Mat var_Ip = mean_Ip - mean_I.mul(mean_p);
		//���������������ϵ��a,b
		cv::Mat a = cov_Ip / (var_I + eps);
		cv::Mat b = mean_p - a.mul(mean_I);
		//�����ģ�����ϵ��a\b�ľ�ֵ
		cv::Mat mean_a;
		cv::boxFilter(a, mean_a, CV_64FC1, cv::Size(r, r));
		mean_a = mean_a / boxResult;
		cv::Mat mean_b;
		cv::boxFilter(b, mean_b, CV_64FC1, cv::Size(r, r));
		mean_b = mean_b / boxResult;
		//�����壺�����������
		outPutImage = mean_a.mul(srcImage) + mean_b;
	}
	catch (cv::Exception& e)
	{
		throw e;
	}
}

void  GammaTransform(cv::Mat &image, cv::Mat &dist, double gamma)
{

	cv::Mat imageGamma;
	//�Ҷȹ�һ��
	image.convertTo(imageGamma, CV_64F, 1.0 / 255, 0);

	//٤���任
	cv::pow(imageGamma, gamma, dist);//dist Ҫ��imageGamma����ͬ����������

	dist.convertTo(dist, CV_8U, 255, 0);
}