#include "interpolation.hpp"

glm::vec3
interpolation::evalLERP(glm::vec3 const& p0, glm::vec3 const& p1, float const x)
{
	//! \todo Implement this function

	glm::vec3 ret = glm::vec2(1.0f, x) * glm::mat2x2(1.0f, -1.0f, 0.0f, 1.0f) * glm::mat3x2(p0.x, p1.x, p0.y, p1.y, p0.z, p1.z);

	return ret;
}

glm::vec3
interpolation::evalCatmullRom(glm::vec3 const& p0, glm::vec3 const& p1,
                              glm::vec3 const& p2, glm::vec3 const& p3,
                              float const t, float const x)
{

	glm::mat4x3 points = glm::mat3x4(p0.x, p1.x, p2.x, p3.x, p0.y, p1.y, p2.y, p3.y, p0.z, p1.z, p2.z, p3.z);
	glm::mat4x4 tension = glm::mat4x4({ 0.0f, -t, 2 * t, -t }, { 1.0f, 0.0f, t - 3, 2 - t }, { 0.0f, t, 3 - 2 * t, t - 2 }, { 0.0f, 0.0f, -t, t });
	glm::vec4 spline = glm::vec4(1.0f, x, x * x, x * x * x);
	glm::vec3 ret = spline * tension * points;

	return ret;
}
