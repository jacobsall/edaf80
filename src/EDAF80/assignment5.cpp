#include "assignment5.hpp"
#include "parametric_shapes.hpp"

#include "config.hpp"
#include "core/Bonobo.h"
#include "core/FPSCamera.h"
#include "core/helpers.hpp"
#include "core/ShaderProgramManager.hpp"
#include "core/node.hpp"
#include <imgui.h>
#include <tinyfiledialogs.h>

#include <clocale>
#include <stdexcept>
#include <glm/gtc/type_ptr.hpp>

int mapSize = 100;

edaf80::Assignment5::Assignment5(WindowManager& windowManager) :
	mCamera(0.5f * glm::half_pi<float>(),
	        static_cast<float>(config::resolution_x) / static_cast<float>(config::resolution_y),
	        0.01f, 1000.0f),
	inputHandler(), mWindowManager(windowManager), window(nullptr)
{
	WindowManager::WindowDatum window_datum{ inputHandler, mCamera, config::resolution_x, config::resolution_y, 0, 0, 0, 0};

	window = mWindowManager.CreateGLFWWindow("EDAF80: Assignment 5", window_datum, config::msaa_rate);
	if (window == nullptr) {
		throw std::runtime_error("Failed to get a window: aborting!");
	}

	bonobo::init();
}

edaf80::Assignment5::~Assignment5()
{
	bonobo::deinit();
}

glm::vec3 edaf80::Assignment5::getRandomPosition() {
	return glm::vec3((rand() % mapSize) - mapSize/2, (rand() % mapSize) - mapSize/2 , (rand() % mapSize) - mapSize/2);
}

void
edaf80::Assignment5::run()
{	
	// Set up the camera
	mCamera.mWorld.SetTranslate(glm::vec3(0.0f, 0.0f, 6.0f));
	mCamera.mMouseSensitivity = 0.003f;
	mCamera.mMovementSpeed = 0.0f; // 3 m/s => 10.8 km/h
	auto camera_position = mCamera.mWorld.GetTranslation();
	glm::vec3 cameraVelocity = glm::vec3(0, 0, 0);

	// Create the shader programs
	ShaderProgramManager program_manager;
	GLuint fallback_shader = 0u;
	program_manager.CreateAndRegisterProgram("Fallback",
	                                         { { ShaderType::vertex, "common/fallback.vert" },
	                                           { ShaderType::fragment, "common/fallback.frag" } },
	                                         fallback_shader);
	if (fallback_shader == 0u) {
		LogError("Failed to load fallback shader");
		return;
	}

	GLuint diffuse_shader = 0u;
	program_manager.CreateAndRegisterProgram("Diffuse",
		{ { ShaderType::vertex, "EDAF80/diffuse.vert" },
		  { ShaderType::fragment, "EDAF80/diffuse.frag" } },
		diffuse_shader);
	if (diffuse_shader == 0u)
		LogError("Failed to load diffuse shader");

	GLuint skybox_shader = 0u;
	program_manager.CreateAndRegisterProgram("Skybox",
		{ { ShaderType::vertex, "EDAF80/skybox.vert" },
		  { ShaderType::fragment, "EDAF80/skybox.frag" } },
		skybox_shader);
	if (skybox_shader == 0u)
		LogError("Failed to load skybox shader");

	GLuint cross_shader = 0u;
	program_manager.CreateAndRegisterProgram("Cross",
		{ { ShaderType::vertex, "EDAF80/cross.vert" },
		  { ShaderType::fragment, "EDAF80/cross.frag" } },
		cross_shader);
	if (cross_shader == 0u)
		LogError("Failed to load cross shader");

	GLuint phong_shader = 0u;
	program_manager.CreateAndRegisterProgram("Phong",
		{ { ShaderType::vertex, "EDAF80/phong.vert" },
		  { ShaderType::fragment, "EDAF80/phong.frag" } },
		phong_shader);
	if (phong_shader == 0u)
		LogError("Failed to load phong shader");

	bool use_normal_mapping = true;
	float ellapsed_time_s = 0.0f;
	auto light_position = glm::vec3(-16.0f, 4.0f, 16.0f);
	auto ambient = glm::vec3(0.1f, 0.1f, 0.1f);
	auto diffuse = glm::vec3(132.0f / 255.0f, 132.0f / 255.0f, 119.0f / 255.0f);
	auto specular = glm::vec3(0.0f,0.0f,0.0f);
	auto shininess = 1.0f;
	auto const phong_set_uniforms = [&use_normal_mapping, &light_position, &camera_position, &ambient, &diffuse, &specular, &shininess](GLuint program) {
		glUniform1i(glGetUniformLocation(program, "use_normal_mapping"), use_normal_mapping ? 1 : 0);
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position));
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(camera_position));
		glUniform3fv(glGetUniformLocation(program, "ambient"), 1, glm::value_ptr(ambient));
		glUniform3fv(glGetUniformLocation(program, "diffuse"), 1, glm::value_ptr(diffuse));
		glUniform3fv(glGetUniformLocation(program, "specular"), 1, glm::value_ptr(specular));
		glUniform1f(glGetUniformLocation(program, "shininess"), shininess);
	};

	auto const set_uniforms = [&light_position, &camera_position, &ellapsed_time_s](GLuint program) {
		glUniform3fv(glGetUniformLocation(program, "light_position"), 1, glm::value_ptr(light_position));
		glUniform3fv(glGetUniformLocation(program, "camera_position"), 1, glm::value_ptr(camera_position));
		glUniform1f(glGetUniformLocation(program, "t"), ellapsed_time_s);
	};


	//
	// Todo: Load your geometry
	//

	auto skybox_cube_map = bonobo::loadTextureCubeMap(
		config::resources_path("cubemaps/NissiBeach2/posx.jpg"),
		config::resources_path("cubemaps/NissiBeach2/negx.jpg"),
		config::resources_path("cubemaps/NissiBeach2/posy.jpg"),
		config::resources_path("cubemaps/NissiBeach2/negy.jpg"),
		config::resources_path("cubemaps/NissiBeach2/posz.jpg"),
		config::resources_path("cubemaps/NissiBeach2/negz.jpg"));

	auto normal_map_id = bonobo::loadTexture2D(
		config::resources_path("textures/asteroid_normal.jpg"));
	auto diffuse_tex_id = bonobo::loadTexture2D(
		config::resources_path("textures/asteroid.jpg"));



	auto skybox_shape = parametric_shapes::createSphere(mapSize, 100u, 100u);
	if (skybox_shape.vao == 0u) {
		LogError("Failed to retrieve the mesh for the skybox");
		return;
	}

	Node skybox;
	skybox.set_geometry(skybox_shape);
	skybox.set_program(&skybox_shader, set_uniforms);
	skybox.add_texture("my_cube_map", skybox_cube_map, GL_TEXTURE_CUBE_MAP);

	

	struct Asteroid{
		Node node;
		glm::vec3 v;
		glm::vec3 trajetory;
		float speed;
		float radius;
		float angle;
		glm::vec3 rotationDirection;
	};

	auto asteroids = std::vector<Asteroid>(20);
	Node cross;
	cross.set_geometry(parametric_shapes::createCross(0.003f, 0.003f, 2.5f));
	cross.set_program(&cross_shader, set_uniforms);

	for (int i = 0; i < asteroids.size(); i++)
	{
		Asteroid asteroid;

		auto size = (double)rand() / ((double)RAND_MAX + 1);
		asteroid.radius = size;
		asteroid.angle = 0.0f;
		asteroid.rotationDirection = glm::normalize(getRandomPosition());

		auto asteroidShape = parametric_shapes::createSphere(asteroid.radius, 60u, 60u);
		asteroid.node.set_geometry(asteroidShape);
		asteroid.node.set_program(&phong_shader, phong_set_uniforms);
		asteroid.node.add_texture("normal_map", normal_map_id, GL_TEXTURE_2D);
		asteroid.node.add_texture("diffuse_texture", diffuse_tex_id, GL_TEXTURE_2D);
		asteroid.node.get_transform().SetTranslate(getRandomPosition());
		asteroid.v = glm::vec3(0.0f, 0.0f, 0.0f);
		asteroid.trajetory = glm::normalize(getRandomPosition());
		asteroid.speed = 2;

		asteroids[i] = asteroid;
	}

	glClearDepthf(1.0f);
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glEnable(GL_DEPTH_TEST);


	auto lastTime = std::chrono::high_resolution_clock::now();

	bool show_logs = true;
	bool show_gui = true;
	bool shader_reload_failed = false;
	bool show_basis = false;
	float basis_thickness_scale = 1.0f;
	float basis_length_scale = 1.0f;

	while (!glfwWindowShouldClose(window)) {
		auto const nowTime = std::chrono::high_resolution_clock::now();
		auto const deltaTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(nowTime - lastTime);
		lastTime = nowTime;
		ellapsed_time_s += std::chrono::duration<float>(deltaTimeUs).count();

		auto& io = ImGui::GetIO();
		inputHandler.SetUICapture(io.WantCaptureMouse, io.WantCaptureKeyboard);
		glfwPollEvents();
		inputHandler.Advance();
		mCamera.Update(deltaTimeUs, inputHandler);

		if (inputHandler.GetKeycodeState(GLFW_KEY_R) & JUST_PRESSED) {
			shader_reload_failed = !program_manager.ReloadAllPrograms();
			if (shader_reload_failed)
				tinyfd_notifyPopup("Shader Program Reload Error",
				                   "An error occurred while reloading shader programs; see the logs for details.\n"
				                   "Rendering is suspended until the issue is solved. Once fixed, just reload the shaders again.",
				                   "error");
		}
		if (inputHandler.GetKeycodeState(GLFW_KEY_F3) & JUST_RELEASED)
			show_logs = !show_logs;
		if (inputHandler.GetKeycodeState(GLFW_KEY_F2) & JUST_RELEASED)
			show_gui = !show_gui;
		if (inputHandler.GetKeycodeState(GLFW_KEY_F11) & JUST_RELEASED)
			mWindowManager.ToggleFullscreenStatusForWindow(window);


		// Retrieve the actual framebuffer size: for HiDPI monitors,
		// you might end up with a framebuffer larger than what you
		// actually asked for. For example, if you ask for a 1920x1080
		// framebuffer, you might get a 3840x2160 one instead.
		// Also it might change as the user drags the window between
		// monitors with different DPIs, or if the fullscreen status is
		// being toggled.
		int framebuffer_width, framebuffer_height;
		glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
		glViewport(0, 0, framebuffer_width, framebuffer_height);


		// INPUT HANDLING
		float move = 0;
		float strafe = 0;
		float deltaTime = std::chrono::duration<float>(deltaTimeUs).count();
		if (inputHandler.GetKeycodeState(GLFW_KEY_W) & PRESSED)
			move = 0.2;
		if (inputHandler.GetKeycodeState(GLFW_KEY_A) & PRESSED)
			strafe = -0.2;
		if (inputHandler.GetKeycodeState(GLFW_KEY_S) & PRESSED)
			move = -0.2;
		if (inputHandler.GetKeycodeState(GLFW_KEY_D) & PRESSED)
			strafe = 0.2;

		float maxVelocity = 2;
		if (glm::length(cameraVelocity) >= maxVelocity) {
			move = 0;
			strafe = 0;
		}
		cameraVelocity = cameraVelocity + mCamera.mWorld.GetFront() * move * deltaTime + mCamera.mWorld.GetRight() * strafe * deltaTime;
		cameraVelocity = cameraVelocity + -cameraVelocity * 1.5f * deltaTime;
		
		if (inputHandler.GetKeycodeState(GLFW_KEY_SPACE) & JUST_RELEASED) {
			for (int i = 0; i < asteroids.size(); i++)
			{
				glm::vec3 ps = asteroids[i].node.get_transform().GetTranslation();
				glm::vec3 pv = mCamera.mWorld.GetTranslation();
				glm::vec3 v = mCamera.mWorld.GetFront();
				glm::vec3 u = ps - pv;
				if (glm::length(u - v * glm::dot(u, v)) < asteroids[i].radius) {
					asteroids[i].node.get_transform().SetTranslate(getRandomPosition());
				}
			}
			
		}

		/*if (glm::length(cameraVelocity) < 0.1f) {
			cameraVelocity = glm::vec3(0, 0, 0);
		}*/
			

		
		mCamera.mWorld.Translate(cameraVelocity);




		//mCamera.mWorld.LookAt(player.get_transform().GetTranslation());
		mWindowManager.NewImGuiFrame();

		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
		

		if (!shader_reload_failed) {
			//
			// Todo: Render all your geometry here.
			//

			//Astroid loop
			for (int i = 0; i < asteroids.size(); i++)
			{
				if (glm::length(asteroids[i].node.get_transform().GetTranslation()) >= mapSize) {
					asteroids[i].trajetory = -asteroids[i].trajetory;
					asteroids[i].node.get_transform().Translate(asteroids[i].trajetory* asteroids[i].speed* deltaTime);
				}
				asteroids[i].v = asteroids[i].trajetory * asteroids[i].speed * deltaTime;
				asteroids[i].node.get_transform().Translate(asteroids[i].v);
				asteroids[i].angle = asteroids[i].angle + 1 * deltaTime;
				asteroids[i].node.get_transform().SetRotate(asteroids[i].angle, asteroids[i].trajetory);


				//Astroid Collision
				for (int j = 0; j < asteroids.size(); j++) {
					if (j == i) continue;

					glm::vec3 p1 = asteroids[i].node.get_transform().GetTranslation();
					glm::vec3 p2 = asteroids[j].node.get_transform().GetTranslation();
					float r1 = asteroids[i].radius;
					float r2 = asteroids[j].radius;

					if (glm::length(p1 - p2) < r1 + r2) {
						auto n = normalize(p1 - p2);
						auto u = reflect(asteroids[i].trajetory, -n);
						auto v = reflect(asteroids[j].trajetory, -n);

						asteroids[i].trajetory = u;
						asteroids[j].trajetory = v;
					}
				}

				asteroids[i].node.render(mCamera.GetWorldToClipMatrix());
			}

			
			skybox.render(mCamera.GetWorldToClipMatrix());
			cross.get_transform().SetTranslate(mCamera.mWorld.GetTranslation() + mCamera.mWorld.GetFront() * 0.1f);
			cross.get_transform().LookAt(mCamera.mWorld.GetTranslation());
			cross.render(mCamera.GetWorldToClipMatrix());
		}


		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		//
		// Todo: If you want a custom ImGUI window, you can set it up
		//       here
		//
		bool const opened = ImGui::Begin("Scene Controls", nullptr, ImGuiWindowFlags_None);
		if (opened) {
			ImGui::Checkbox("Show basis", &show_basis);
			ImGui::SliderFloat("Basis thickness scale", &basis_thickness_scale, 0.0f, 100.0f);
			ImGui::SliderFloat("Basis length scale", &basis_length_scale, 0.0f, 100.0f);
		}
		ImGui::End();

		if (show_basis)
			bonobo::renderBasis(basis_thickness_scale, basis_length_scale, mCamera.GetWorldToClipMatrix());
		if (show_logs)
			Log::View::Render();
		mWindowManager.RenderImGuiFrame(show_gui);

		glfwSwapBuffers(window);
	}
}

int main()
{
	std::setlocale(LC_ALL, "");

	Bonobo framework;

	try {
		edaf80::Assignment5 assignment5(framework.GetWindowManager());
		assignment5.run();
	} catch (std::runtime_error const& e) {
		LogError(e.what());
	}
}
