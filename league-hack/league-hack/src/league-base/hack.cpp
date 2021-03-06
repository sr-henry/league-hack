#include "hack.h"

Hack::Hack()
{
	hGameWindow = FindWindow(NULL, "League of Legends (TM) Client");
	if (!hGameWindow)
		exit(1);

	esLocalPlayer = {
		vec2{43, 115},
		cv::Scalar(92, 212, 0),
		cv::Scalar(97, 255, 79),
		cv::getStructuringElement(cv::MORPH_RECT, cv::Size(5, 5)),
		cv::getStructuringElement(cv::MORPH_RECT, cv::Size(8, 8)),
		4
	};

	esEnemy = {
		vec2{40, 57},
		cv::Scalar(114, 236, 44),
		cv::Scalar(119, 255, 67),
		cv::getStructuringElement(cv::MORPH_RECT, cv::Size(6, 6)),
		cv::getStructuringElement(cv::MORPH_RECT, cv::Size(9, 9)),
		4
	};

	utils = Utils(hGameWindow);

	tp1 = std::chrono::system_clock::now();
	tp2 = std::chrono::system_clock::now();

	enemiesList.reserve(5);
	p_enemiesList.reserve(5);

	std::cout << "Hack init success!\n";
}

void Hack::Update()
{
	tp2 = std::chrono::system_clock::now();
	elapsedTime = tp2 - tp1;
	tp1 = tp2;
	fElapsedTime = elapsedTime.count();
	gameTime += fElapsedTime;

	f = std::async(
		std::launch::async,
		HttpRequestGet,
		"https://127.0.0.1:2999/liveclientdata/allgamedata",
		&sAllGameData
	);

	WindowCapture();
	GetLocalPlayer();
	GetLocalPlayerData();
	GetEnemies();
	GetEnemiesData();
}

void Hack::GetLocalPlayer()
{
	cv::Mat mask;
	cv::inRange(mGameImage, esLocalPlayer.l, esLocalPlayer.u, mask);

	cv::morphologyEx(mask, mask, cv::MORPH_OPEN, esLocalPlayer.k0);

	cv::dilate(mask, mask, esLocalPlayer.k1);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(mask, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

	std::vector<std::vector<cv::Point>> approx(contours.size());

	cv::Rect boundingBox;

	for (size_t i = 0; i < contours.size(); i++)
	{
		cv::approxPolyDP(contours[i], approx[i], 0.1f * cv::arcLength(contours[i], true), true);

		if (approx[i].size() == esLocalPlayer.threshold)
		{
			boundingBox = cv::boundingRect(approx[i]);

			float rx = static_cast<float>(boundingBox.x + boundingBox.width);
			float ry = static_cast<float>(boundingBox.y + boundingBox.height);

			b_localPlayer = { vec2{rx, ry} + esLocalPlayer.offset };
		
			localPlayer = &b_localPlayer;
			return;
		}
	}
	localPlayer = nullptr;
}

void Hack::GetLocalPlayerData()
{
	if (sAllGameData.empty())
		return;

	json j;

	try {
		j = json::parse(sAllGameData);
	}
	catch (json::parse_error& ex) {
		std::cout << ex.what() << std::endl;
		std::cout << ex.id << std::endl;
		return;
	}

	b_localPlayer.stats = j["activePlayer"]["championStats"];

	float a = h / w;

	float fScreenAARangeRadius = b_localPlayer.stats.attackRange * (1 - a) + 40.f;

	b_localPlayer.vScreenAARange.x = fScreenAARangeRadius;
	b_localPlayer.vScreenAARange.y = fScreenAARangeRadius * cos(35.f * 3.1415f / 180.f);

}

void Hack::GetEnemies()
{
	if (!enemiesList.empty()) {
		p_enemiesList.swap(enemiesList);
		enemiesList.clear();
	}

	cv::Mat mask;
	cv::inRange(mGameImage, esEnemy.l, esEnemy.u, mask);

	cv::morphologyEx(mask, mask, cv::MORPH_OPEN, esEnemy.k0);

	cv::dilate(mask, mask, esEnemy.k1);

	std::vector<std::vector<cv::Point>> contours;
	cv::findContours(mask, contours, cv::RETR_TREE, cv::CHAIN_APPROX_SIMPLE);

	std::vector<std::vector<cv::Point>> approx(contours.size());

	cv::Rect boundingBox;
	int dEnemiesCount = 0;
	for (size_t i = 0; i < contours.size(); i++)
	{
		cv::approxPolyDP(contours[i], approx[i], 0.1f * cv::arcLength(contours[i], true), true);

		if (approx[i].size() == esEnemy.threshold)
		{
			boundingBox = cv::boundingRect(approx[i]);

			float rx = static_cast<float>(boundingBox.x + boundingBox.width);
			float ry = static_cast<float>(boundingBox.y + boundingBox.height);

			enemiesList.emplace_back(dEnemiesCount, vec2{ rx, ry } + esEnemy.offset);

			dEnemiesCount++;
		}
	}
}

void Hack::GetEnemiesData()
{
	int m_size = (p_enemiesList.size() < enemiesList.size()) ? p_enemiesList.size() : enemiesList.size();

	for (int i = 0; i < m_size; i++)
	{
		enemiesList[i].direction = enemiesList[i].pos - p_enemiesList[i].pos;
		enemiesList[i].magnitude = sqrt(enemiesList[i].direction.x * enemiesList[i].direction.x + enemiesList[i].direction.y * enemiesList[i].direction.y);
		enemiesList[i].speed = enemiesList[i].magnitude / fElapsedTime;
	}
}

Enemy* Hack::GetClosestEnemy(vec2 ref)
{
	float fMaxDist = 9999.0f;
	vec2 vDistance;
	float fDistance;

	if (enemiesList.size() == 0)
		return nullptr;

	for (int i = 0; i < enemiesList.size(); i++) {

		vDistance = enemiesList[i].pos - ref;
		fDistance = sqrt(vDistance.x * vDistance.x + vDistance.y * vDistance.y);

		if (fDistance < fMaxDist) {
			fMaxDist = fDistance;
			closestEnemy = enemiesList[i];
		}

	}

	return &closestEnemy;
}

bool Hack::IsGameRunning()
{
	if (hGameWindow == GetForegroundWindow())
		return true;
	return false;
}

void Hack::WindowCapture()
{
	HDC hdc_target, hdc;

	RECT rWindowRect;
	GetClientRect(hGameWindow, &rWindowRect);

	int nWindowX = rWindowRect.left;
	int nWindowY = rWindowRect.top;
	int nWindowH = rWindowRect.bottom;
	int nWindowW = rWindowRect.right;

	h = static_cast<float>(nWindowH); w = static_cast<float>(nWindowW);

	POINT p = { nWindowX, nWindowY };
	ClientToScreen(hGameWindow, &p);

	hdc_target = GetDC(NULL);
	hdc = CreateCompatibleDC(NULL);

	mGameImage = cv::Mat(nWindowH, nWindowW, CV_8UC4);

	HBITMAP bmp = CreateCompatibleBitmap(hdc_target, nWindowW, nWindowH);

	SelectObject(hdc, bmp);

	BitBlt(hdc, 0, 0, nWindowW, nWindowH, hdc_target, p.x, p.y, SRCCOPY);

	ReleaseDC(NULL, hdc_target);

	BITMAPINFO bi = { 0 };
	BITMAPINFOHEADER* bih = &bi.bmiHeader;
	bih->biSize = sizeof(BITMAPINFOHEADER);
	bih->biBitCount = 32;
	bih->biWidth = nWindowW;
	bih->biHeight = -nWindowH;
	bih->biPlanes = 1;

	GetDIBits(hdc, bmp, 0, nWindowH, mGameImage.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);

	DeleteObject(bmp);
	DeleteDC(hdc);

	cv::cvtColor(mGameImage, mGameImage, cv::COLOR_RGBA2RGB);
	cv::cvtColor(mGameImage, mGameImage, cv::COLOR_RGB2HSV);
}