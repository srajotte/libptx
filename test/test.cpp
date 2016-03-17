#include "../libptx/libptx.h"

#include <vector>
#include <iostream>

typedef std::vector<libptx::Point> PointList;

class PointInserter : public libptx::IPointInserter
{
public:
	PointInserter(PointList& points) : m_points(points) {};
	virtual void insert(const libptx::Point& point) override;

private:
	PointList& m_points;
};

class TestStats
{
public:
	TestStats() : m_pass(0), m_fail(0) {};

	float successRate() const { return static_cast<float>(m_pass) / totalTests(); }
	int totalTests() const { return m_pass + m_fail; };

	void addPass() { ++m_pass; };
	void addFail() { ++m_fail; };

private:
	int m_pass;
	int m_fail;
};

void PointInserter::insert(const libptx::Point& point)
{
	if (!point.unsampled())
	{
		m_points.emplace_back(point);
	}
}

void test(bool pass, const std::string test, TestStats& stats)
{
	std::cout << test << " : ";
	pass ? std::cout << "PASS" : std::cout << "FAIL";
	pass ? stats.addPass() : stats.addFail();
	std::cout << std::endl;
}

int wmain(int argc, wchar_t* argv[])
{
	libptx::File file(argv[1]);
	
	std::vector<PointList> scans;
	std::vector<libptx::ScanInfo> scansInfo;

	auto callback = [&scans, &scansInfo](const libptx::ScanInfo& info)
	{
		scansInfo.emplace_back(info);
		scans.emplace_back();
		PointList& points = scans.back();
		points.reserve(info.dimensions.columns * info.dimensions.rows);
		return std::make_shared<PointInserter>(points);
	};

	file.readScans(callback);

	TestStats stats;
	test(scans.size() == 2, "Scan count", stats);
	test(scans.at(0).size() == 673, "Scan 0 point count", stats);
	test(scans.at(1).size() == 90, "Scan 1 point count", stats);

	std::cout << "Success rate : " << stats.successRate() * 100 << "%" << std::endl;
}
