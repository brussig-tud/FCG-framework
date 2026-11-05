
//////
//
// Includes
//

// C++ STL
#include <limits>
#include <numeric>

// Framework
#include <FCG/Mesh/mesh_processing.h>



//////
//
// Module-private symbols
//

// Anonymous namespace begin
namespace {

/// Signed twice-area of a projected triangle.
auto turn (const OpenMesh::Vec2d &a, const OpenMesh::Vec2d &b, const OpenMesh::Vec2d &c) -> double {
	return (b[0]-a[0])*(c[1]-a[1]) - (b[1]-a[1])*(c[0]-a[0]);
}

/// Report failure to progress without claiming that the polygon was validated.
auto clippingFailure () -> std::unexpected<fcg::MeshError> {
	return std::unexpected(fcg::MeshError{fcg::MeshErrorCode::ProcessingFailure,
		"Ear cutting cannot progress on the projected polygon"});
}

// Anonymous namespace end
}



//////
//
// Module namespace open
//

// The library top-level namespace.
namespace fcg {



//////
//
// Class implementations
//

////
// EarCutTriangulator

auto EarCutTriangulator::operator() (MeshFaceView face) const -> std::expected<std::vector<MeshTriangle>, MeshError>
{
	// Translate before projection to reduce cancellation for large coordinate offsets
	const auto p = face.positions;
	const auto count = p.size();
	if (count == 3)
		return std::vector<MeshTriangle>{{0, 1, 2}};
	OpenMesh::Vec3d normal(0, 0, 0);
	for (std::size_t i = 0; i < count; ++i)
		normal += OpenMesh::cross(p[i] - p[0], p[(i + 1) % count] - p[0]);
	int drop = 0;
	for (int axis = 1; axis < 3; ++axis)
		if (std::abs(normal[axis]) > std::abs(normal[drop]))
			drop = axis;
	if (!(std::abs(normal[drop]) > 0))
		return clippingFailure();
	std::vector<OpenMesh::Vec2d> points;
	double extent = 0;
	for (const auto &position : p) {
		const auto delta = position - p[0];
		points.emplace_back(delta[(drop + 1) % 3], delta[(drop + 2) % 3]);
		extent = std::max({extent, std::abs(points.back()[0]), std::abs(points.back()[1])});
	}
	const double sign = normal[drop] > 0 ? 1 : -1;
	const double epsilon = 32 * std::numeric_limits<double>::epsilon() * extent * extent;

	// Maintain a linked boundary and refresh only the neighbors of each removed ear
	std::vector<std::size_t> prev(count), next(count);
	std::vector<bool> alive(count, true), ears(count, false);
	for (std::size_t i = 0; i < count; ++i) {
		prev[i] = (i + count - 1) % count;
		next[i] = (i + 1) % count;
	}
	const auto isEar = [&] (std::size_t i)
	{
		const auto a = prev[i], c = next[i];
		if (sign * turn(points[a], points[i], points[c]) <= epsilon) return false;
		for (std::size_t j = 0; j < count; ++j)
			if (alive[j] && j != a && j != i && j != c
				&& sign * turn(points[a], points[i], points[j]) >= -epsilon
				&& sign * turn(points[i], points[c], points[j]) >= -epsilon
				&& sign * turn(points[c], points[a], points[j]) >= -epsilon)
				return false;
		return true;
	};
	for (std::size_t i=0; i<count; ++i)
		ears[i] = isEar(i);
	std::vector<MeshTriangle> result;
	result.reserve(count - 2);
	std::size_t remaining = count;
	while (remaining > 3)
	{
		auto ear = count;
		for (std::size_t i = 0; i < count; ++i)
			if (alive[i] && ears[i]) {
				ear = i;
				break;
			}
		if (ear == count)
			return clippingFailure();
		const auto a = prev[ear], c = next[ear];
		result.push_back({a, ear, c});
		alive[ear] = false;
		next[a] = c;
		prev[c] = a;
		--remaining;
		ears[a] = isEar(a);
		ears[c] = isEar(c);
	}
	for (std::size_t i = 0; i < count; ++i)
		if (alive[i]) {
			result.push_back({i, next[i], next[next[i]]});
			break;
		}

	// Done!
	return result;
}



//////
//
// Function implementations
//

auto detail::meshSplitPlan (std::size_t count, std::span<const MeshTriangle> triangles) -> std::vector<MeshTriangle>
{
	// Match ears by corner identity, independent of the callback's triangle ordering
	std::vector<std::size_t> prev(count), next(count);
	for (std::size_t i = 0; i < count; ++i) {
		prev[i] = (i + count - 1) % count;
		next[i] = (i + 1) % count;
	}
	std::vector<bool> used(triangles.size(), false);
	std::vector<MeshTriangle> result;
	result.reserve(count - 3);
	while (result.size() < count - 3)
		for (std::size_t i = 0; i < triangles.size(); ++i)
			if (!used[i])
				for (std::size_t j = 0; j < 3; ++j)
				{
					const auto a = triangles[i][j], b = triangles[i][(j + 1) % 3], c = triangles[i][(j + 2) % 3];
					if (next[a] != b || next[b] != c) continue;
					result.push_back({a, b, c});
					used[i] = true;
					next[a] = c;
					prev[c] = a;
					if (result.size() == count - 3) return result;
					break;
				}

	// Done!
	return result;
}



//////
//
// Module namespace close
//

// namespace fcg
}
