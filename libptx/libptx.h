#pragma once

#include <array>
#include <functional>
#include <memory>
#include <tuple>

#include "textio.h"

namespace libptx
{
	struct RasterPosition
	{
		RasterPosition() : column(0), row(0) {};
		RasterPosition(int column, int row) : column(column), row(row) {};

		int column;
		int row;
	};

	class Point
	{
	public:
		Point(const RasterPosition& position, double x, double y, double z, double intensity, unsigned char r, unsigned char g, unsigned char b) 
			: position(position), 
			  x(x), y(y), z(z), 
			  intensity(intensity), 
			  r(r), g(g), b(b) {};

		bool unsampled() const { return x == 0.0 && y == 0.0 && z == 0.0; };

	public:
		RasterPosition position;
		double x, y, z;
		double intensity;
		unsigned char r, g, b;
	};

	class IPointInserter
	{
	public:
		virtual void insert(const Point& point) = 0;
	};

	struct RasterDimensions
	{
		RasterDimensions() : columns(0), rows(0) {};
		RasterDimensions(int columns, int rows) : columns(columns), rows(rows) {};

		int columns;
		int rows;
	};

	template <class T, size_t ROW, size_t COL>
	using Matrix = std::array<std::array<T, COL>, ROW>;
	
	typedef Matrix<double, 3, 3> Matrix3d;
	typedef Matrix<double, 1, 3> Vector3d;

	inline Matrix3d identity() { return{ 1.0,0.0,0.0, 0.0,1.0,0.0, 0.0,0.0,1.0 }; }
	inline Vector3d zeros() { return{ 0.0, 0.0, 0.0 }; }

	struct RegistrationParameters
	{
		RegistrationParameters() : rotation(identity()), translation(zeros()) {};
		RegistrationParameters(const Matrix3d& rotation, const Vector3d& translation) : rotation(rotation), translation(translation) {};

		Matrix3d rotation;
		Vector3d translation;
	};

	struct ScanInfo
	{
		ScanInfo() {};
		ScanInfo(const RasterDimensions& dimensions, const RegistrationParameters& registration) : dimensions(dimensions), registration(registration) {};

		RasterDimensions dimensions;
		RegistrationParameters registration;
	};
	
	typedef std::function<std::shared_ptr<IPointInserter>(const ScanInfo& info)> NewScanCallback;

	class File
	{
	public:
		File(const std::wstring& filename);

		inline const void readScans(NewScanCallback callback);

	private:
		inline std::tuple<ScanInfo, bool> readHeader();
		inline void parseLine(const textio::SubString& substr, const RasterPosition& position, IPointInserter& inserter);

		std::wstring m_filename;
		textio::LineReader m_lineReader;
		textio::Tokenizer m_lineTokenizer;
		textio::Tokenizer::TokenList m_tokens;
	};
}

///////////////////////////////////////////////////////////////////////////////

namespace libptx
{
	std::array<double, 3> parseVector3(const std::string& line)
	{
		textio::Tokenizer spaceTokenizer(' ');
		std::vector<textio::SubString> tokens;
		spaceTokenizer.tokenize(textio::SubString(line.cbegin(), line.cend()), tokens);
		return{ std::stod(tokens[0]), std::stod(tokens[1]), std::stod(tokens[2]) };
	}

	std::tuple<ScanInfo,bool> File::readHeader()
	{
		try
		{
			std::string columns = m_lineReader.getline();
			std::string rows = m_lineReader.getline();

			RasterDimensions dimensions(std::stoi(columns), std::stoi(rows));

			std::string scanner_origin = m_lineReader.getline();
			std::string scanner_x_axis = m_lineReader.getline();
			std::string scanner_y_axis = m_lineReader.getline();
			std::string scanner_z_axis = m_lineReader.getline();

			std::string r0 = m_lineReader.getline();
			std::string r1 = m_lineReader.getline();
			std::string r2 = m_lineReader.getline();

			std::string t = m_lineReader.getline();

			textio::Tokenizer spaceTokenizer(' ');
			std::vector<textio::SubString> tokens;

			Matrix3d rotation;
			rotation[0] = parseVector3(r0);
			rotation[1] = parseVector3(r1);
			rotation[2] = parseVector3(r2);

			Vector3d translation = { parseVector3(t) };

			RegistrationParameters registration(rotation, translation);

			return std::make_tuple(ScanInfo(dimensions, registration), true);
		}
		catch (...)
		{
			return  std::make_tuple(ScanInfo(), false);
		}
	}

	File::File(const std::wstring& filename)
		: m_filename(filename),
		m_lineTokenizer(' '),
		m_lineReader(filename)
	{
	}

	const void File::readScans(NewScanCallback callback)
	{
		bool done = false;
		while (!done)
		{
			auto ret = readHeader();
			if (!std::get<1>(ret))
			{
				done = true;
				continue;
			}
			ScanInfo info = std::get<0>(ret);
			auto inserter = callback(info);

			std::size_t totalLines = info.dimensions.columns * info.dimensions.rows;

			std::size_t lineIndex = 0;
			while (lineIndex < totalLines)
			{
				int row = lineIndex % info.dimensions.rows;
				int column = static_cast<int>(lineIndex / info.dimensions.rows);
				RasterPosition position = { column, row };

				auto line = m_lineReader.getline();
				parseLine(line, position, *inserter);
				++lineIndex;
			}

			if (m_lineReader.eof())
			{
				done = true;
			}
		}
	}

	void File::parseLine(const textio::SubString& line, const RasterPosition& position, IPointInserter& inserter)
	{
		m_lineTokenizer.tokenize(line, m_tokens);

		assert(m_tokens.size() == 7);

		Point point
			(
				position,
				textio::stor<double>(m_tokens[0]),
				textio::stor<double>(m_tokens[1]),
				textio::stor<double>(m_tokens[2]),
				textio::stor<double>(m_tokens[3]),
				textio::stou<unsigned char>(m_tokens[4]),
				textio::stou<unsigned char>(m_tokens[5]),
				textio::stou<unsigned char>(m_tokens[6])
				);

		inserter.insert(point);
	}
}