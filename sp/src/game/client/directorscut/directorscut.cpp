//========= Director's Cut - https://github.com/teampopplio/directorscut ============//
//
// Purpose: Director's Cut shared game system.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"

#include "directorscut.h"
#include "imgui_public.h"
#include "direct3d_hook.h"
#include "vgui/ISurface.h"
#include "vgui_controls/Panel.h"
#include "iinput.h"
#include "input.h"
#include "view.h"
#include "viewrender.h"
#include "engine/IClientLeafSystem.h"
#include "view_shared.h"
#include "viewrender.h"
#include "gamestringpool.h"
#include "datacache/imdlcache.h"
#include "networkstringtable_clientdll.h"
#include "debugoverlay_shared.h"
#include "view_scene.h"
#include "in_buttons.h"
#include "clientsteamcontext.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CDirectorsCutSystem g_DirectorsCutSystem;

CDirectorsCutSystem &DirectorsCutGameSystem()
{
	return g_DirectorsCutSystem;
}

char* defaultDagModelName = "models/editor/axis_helper.mdl";
CModelElement* CreateDag(char* modelName, bool add, bool setIndex);

char* defaultLightTextureName = "effects/flashlight001";
CLightCustomEffect* CreateLight(char* modelName, bool add, bool setIndex);

C_PointCamera* CreateCamera(bool add, bool setIndex);

char* defaultRagdollName = "models/alyx.mdl";
CModelElement* CreateRagdoll(char* modelName, bool add, bool setIndex);

CModelElement* MakeRagdoll(CModelElement* dag);
CModelElement* MakeRagdoll(CModelElement* dag, bool add, bool setIndex);

ConVar cvar_imgui_enabled("dx_imgui_enabled", "1", FCVAR_ARCHIVE, "Enable ImGui");
ConVar cvar_imgui_input_enabled("dx_imgui_input_enabled", "1", FCVAR_ARCHIVE, "Capture ImGui input");
ConVar cvar_dx_zoom_distance("dx_zoom_distance", "5.0f", FCVAR_ARCHIVE, "Default zoom distance");
ConVar cvar_dx_orbit_sensitivity("dx_orbit_sensitivity", "0.1f", FCVAR_ARCHIVE, "Middle click orbit sensitivity");
ConVar cvar_dx_pan_sensitivity("dx_pan_sensitivity", "0.1f", FCVAR_ARCHIVE, "Shift + middle click pan sensitivity");

ConCommand cmd_imgui_toggle("dx_imgui_toggle", [](const CCommand& args) {
	cvar_imgui_enabled.SetValue(!cvar_imgui_enabled.GetBool());
}, "Toggle ImGui", FCVAR_ARCHIVE);
ConCommand cmd_imgui_input_toggle("dx_imgui_input_toggle", [](const CCommand& args) {
	cvar_imgui_input_enabled.SetValue(!cvar_imgui_input_enabled.GetBool());
}, "Toggle ImGui input", FCVAR_ARCHIVE);
ConCommand cmd_create_dag("dx_create_dag", [](const CCommand& args) {
	char* modelName = defaultDagModelName;
	if (args.ArgC() > 1)
		modelName = (char*)args.Arg(1);
	CreateDag(modelName, true, true);
}, "Create dag", FCVAR_ARCHIVE);
ConCommand cmd_create_ragdoll("dx_create_ragdoll", [](const CCommand& args) {
	char* modelName = defaultRagdollName;
	if (args.ArgC() > 1)
		modelName = (char*)args.Arg(1);
	CreateRagdoll(modelName, true, true);
}, "Create ragdoll", FCVAR_ARCHIVE);

ConCommand cmd_in_select_elements("+dx_select", [](const CCommand& args) {
	g_DirectorsCutSystem.selecting = true;
}, "Select elements", FCVAR_ARCHIVE);

ConCommand cmd_out_select_elements("-dx_select", [](const CCommand& args) {
	g_DirectorsCutSystem.selecting = false;
}, "Select elements", FCVAR_ARCHIVE);

WNDPROC ogWndProc = nullptr;

static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::TRANSLATE | ImGuizmo::ROTATE);
static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);

// Render Dear ImGui
void APIENTRY EndScene(LPDIRECT3DDEVICE9 p_pDevice)
{
	// Initialization
	bool firstFrame = false;
	if (g_DirectorsCutSystem.firstEndScene)
	{
		g_DirectorsCutSystem.firstEndScene = false;
		firstFrame = true;
		if(g_DirectorsCutSystem.imguiActive)
			ImGui_ImplDX9_Init(p_pDevice);
	}
	// Only render Imgui when enabled
	if (cvar_imgui_enabled.GetBool() && g_DirectorsCutSystem.imguiActive && !engine->IsPaused())
	{
		ImGuiIO& io = ImGui::GetIO();

		int windowWidth;
		int windowHeight;
		vgui::surface()->GetScreenSize(windowWidth, windowHeight);

		// Drawing
		
		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();

		ImGui::NewFrame();
		ImGuizmo::BeginFrame();

		// add menu buttons at top
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				ImGui::Text("***** DUMMY MENU *****");
				if (ImGui::MenuItem("New"))
				{
				}
				if (ImGui::MenuItem("Open"))
				{

				}
				if (ImGui::MenuItem("Save"))
				{
				}
				if (ImGui::MenuItem("Save As"))
				{
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				ImGui::Text("***** DUMMY MENU *****");
				if (ImGui::MenuItem("Undo"))
				{
				}
				if (ImGui::MenuItem("Redo"))
				{
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View"))
			{
				// windows -> show/hide camera window, show/hide element window, show/hide inspector window
				ImGui::Text("***** DUMMY MENU *****");
				bool dummy = true;
				ImGui::Checkbox("Camera", &dummy);
				ImGui::Checkbox("Elements", &dummy);
				ImGui::Checkbox("Inspector", &dummy);
				ImGui::EndMenu();
			}
			if(ImGui::BeginMenu("About"))
			{
				// external link buttons
				if (ImGui::MenuItem("GitHub"))
				{
					// I tried this but it didn't work, oh well
					//ClientSteamContext().SteamFriends()->ActivateGameOverlayToWebPage("https://github.com/TeamPopplio/directorscut");
					ShellExecute(0, 0, "https://github.com/TeamPopplio/directorscut", 0, 0 , SW_SHOW );
				}
				if (ImGui::MenuItem("Twitter"))
				{
					ShellExecute(0, 0, "https://twitter.com/SFMDirectorsCut", 0, 0 , SW_SHOW );
				}
				ImGui::Text("Version: %s", g_DirectorsCutSystem.directorcut_version.GetVersion());
				ImGui::EndMenu();
			}
			ImGui::EndMainMenuBar();
		}

		// element viewer (tree window)
		// set up window
		ImGui::SetNextWindowPos(ImVec2(windowWidth - 8, 19 + 8), 0, ImVec2(1.0f, 0.0f));
		ImGui::SetNextWindowSize(ImVec2(264, windowHeight - 96 - 19 - 24), 0);
		ImGui::Begin("Inspector", 0, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_HorizontalScrollbar);

		// root tree node
		if (ImGui::TreeNodeEx("Root", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::TreeNodeEx("Models", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (int i = 0; i < g_DirectorsCutSystem.dags.Count(); i++)
				{
					CModelElement* dag = g_DirectorsCutSystem.dags[i];
					// use i as name for tree node
					// special case for models: use model name
					char name[MAXCHAR];
					sprintf(name, "%d (%s)", i, dag->GetModelName());
					if (ImGui::TreeNode(name))
					{
						if (ImGui::Button("Select"))
						{
							g_DirectorsCutSystem.elementMode = 0; // model mode
							g_DirectorsCutSystem.elementIndex = i;
							g_DirectorsCutSystem.boneIndex = -1;
							g_DirectorsCutSystem.nextPoseIndex = -1;
						}
						// show position
						Vector pos = dag->GetAbsOrigin();
						ImGui::Text("Position: %.2f %.2f %.2f", pos.x, pos.y, pos.z);
						// show rotation
						QAngle rot = dag->GetAbsAngles();
						ImGui::Text("Rotation: %.2f %.2f %.2f", rot.x, rot.y, rot.z);
						CStudioHdr* modelPtr = dag->GetModelPtr();
						if (modelPtr)
						{
							// show physics objects (ragdolls only)
							if (dag->IsRagdoll())
							{
								if (ImGui::TreeNode("Physics Objects"))
								{
									CRagdoll* ragdoll = dag->m_pRagdoll;
									if (ragdoll)
									{
										for (int j = 0; j < dag->m_pRagdoll->RagdollBoneCount(); j++)
										{
											IPhysicsObject* physObj = ragdoll->GetElement(j);
											// use j as name for tree node
											// special case for physics objects: use bone name
											for (int k = 0; k < modelPtr->numbones(); k++)
											{
												mstudiobone_t* bone = modelPtr->pBone(k);
												if (bone != nullptr)
												{
													if (bone->physicsbone == j)
													{
														char name[MAXCHAR];
														sprintf(name, "%d (%s)", j, bone->pszName());
														if (ImGui::TreeNode(name))
														{
															if (ImGui::Button("Select"))
															{
																g_DirectorsCutSystem.elementMode = 0;
																g_DirectorsCutSystem.elementIndex = i;
																g_DirectorsCutSystem.boneIndex = j;
																g_DirectorsCutSystem.nextPoseIndex = -1;
															}
															// show position
															Vector worldVec;
															QAngle worldAng;
															physObj->GetPosition(&worldVec, &worldAng);
															ImGui::Text("Position: %.2f %.2f %.2f", worldVec.x, worldVec.y, worldVec.z);
															// show rotation
															ImGui::Text("Rotation: %.2f %.2f %.2f", worldAng.x, worldAng.y, worldAng.z);
															ImGui::TreePop();
														}
														break;
													}
												}
											}
										}
										ImGui::TreePop();
									}
								}
							}
							// show pose bones
							if (ImGui::TreeNode("Pose Bones"))
							{
								for (int j = 0; j < modelPtr->numbones(); j++)
								{
									mstudiobone_t* bone = modelPtr->pBone(j);
									if (bone != nullptr)
									{
										char name[MAXCHAR];
										sprintf(name, "%d (%s)", j, bone->pszName());
										if (ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_Bullet | ImGuiTreeNodeFlags_Leaf))
										{
											if (ImGui::IsItemClicked())
											{
												g_DirectorsCutSystem.elementMode = 0;
												g_DirectorsCutSystem.elementIndex = i;
												g_DirectorsCutSystem.boneIndex = -1;
												g_DirectorsCutSystem.nextPoseIndex = j;
											}
											// FIXME: can't show position in drawing code!
											ImGui::TreePop();
										}
									}
								}
								ImGui::TreePop();
							}
						}
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
			if (ImGui::TreeNodeEx("Lights", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (int i = 0; i < g_DirectorsCutSystem.lights.Count(); i++)
				{
					CLightCustomEffect* dag = g_DirectorsCutSystem.lights[i];
					// use i as name for tree node
					char name[10];
					sprintf(name, "%d", i);
					if (ImGui::TreeNode(name))
					{
						if (ImGui::Button("Select"))
						{
							g_DirectorsCutSystem.elementMode = 1; // light mode
							g_DirectorsCutSystem.elementIndex = i;
							g_DirectorsCutSystem.boneIndex = -1;
							g_DirectorsCutSystem.nextPoseIndex = -1;
						}
						// show position
						Vector pos = dag->GetAbsOrigin();
						ImGui::Text("Position: %.2f %.2f %.2f", pos.x, pos.y, pos.z);
						// show rotation
						QAngle rot = dag->GetAbsAngles();
						ImGui::Text("Rotation: %.2f %.2f %.2f", rot.x, rot.y, rot.z);
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}

			if (ImGui::TreeNodeEx("Cameras", ImGuiTreeNodeFlags_DefaultOpen))
			{
				for (int i = 0; i < g_DirectorsCutSystem.cameras.Count(); i++)
				{
					C_PointCamera* dag = g_DirectorsCutSystem.cameras[i];
					// use i as name for tree node
					char name[10];
					sprintf(name, "%d", i);
					if (ImGui::TreeNode(name))
					{
						if (ImGui::Button("Select"))
						{
							g_DirectorsCutSystem.elementMode = 2; // light mode
							g_DirectorsCutSystem.elementIndex = i;
							g_DirectorsCutSystem.boneIndex = -1;
							g_DirectorsCutSystem.nextPoseIndex = -1;
						}
						// show position
						Vector pos = dag->GetAbsOrigin();
						ImGui::Text("Position: %.2f %.2f %.2f", pos.x, pos.y, pos.z);
						// show rotation
						QAngle rot = dag->GetAbsAngles();
						ImGui::Text("Rotation: %.2f %.2f %.2f", rot.x, rot.y, rot.z);
						ImGui::TreePop();
					}
				}
				ImGui::TreePop();
			}
		}

		// end window
		ImGui::End();

		// Perspective camera view
		// Orthographic mode is not supported
		const CViewSetup* viewSetup = view->GetViewSetup();
		float fov = viewSetup->fov * g_DirectorsCutSystem.fovAdjustment;
		g_DirectorsCutSystem.Perspective(fov, io.DisplaySize.x / io.DisplaySize.y, 0.01f, viewSetup->zFar, g_DirectorsCutSystem.cameraProjection); //viewSetup->zNear, viewSetup->zFar, g_DirectorsCutSystem.cameraProjection);

		ImGuizmo::SetOrthographic(g_DirectorsCutSystem.orthographic);

		ImGui::Begin("Camera", 0, ImGuiWindowFlags_AlwaysAutoResize);

		ImGui::LabelText("Origin", "%.0f %.0f %.0f", g_DirectorsCutSystem.engineOrigin.x, g_DirectorsCutSystem.engineOrigin.y, g_DirectorsCutSystem.engineOrigin.z);
		ImGui::LabelText("Angles", "%.0f %.0f %.0f", g_DirectorsCutSystem.engineAngles.x, g_DirectorsCutSystem.engineAngles.y, g_DirectorsCutSystem.engineAngles.z);

		float pivot[3];
		pivot[0] = g_DirectorsCutSystem.pivot.x;
		pivot[1] = g_DirectorsCutSystem.pivot.y;
		pivot[2] = g_DirectorsCutSystem.pivot.z;
		ImGui::InputFloat3("Pivot", pivot);
		g_DirectorsCutSystem.pivot.x = pivot[0];
		g_DirectorsCutSystem.pivot.y = pivot[1];
		g_DirectorsCutSystem.pivot.z = pivot[2];
		

		if (ImGui::Button("Map Origin to Pivot (O)") || ImGui::IsKeyPressed(ImGuiKey_O, false))
			g_DirectorsCutSystem.pivot = Vector(0, 0, 0);

		if (ImGui::Button("Player Eyes to Pivot (P)") || ImGui::IsKeyPressed(ImGuiKey_P, false))
			g_DirectorsCutSystem.pivot = g_DirectorsCutSystem.playerOrigin;

		// Failsafes for element modes
		if (g_DirectorsCutSystem.nextElementMode != g_DirectorsCutSystem.elementMode)
		{
			g_DirectorsCutSystem.elementMode = g_DirectorsCutSystem.nextElementMode;
			switch (g_DirectorsCutSystem.elementMode)
			{
			case 0:
				g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.dags.Count() - 1;
			case 1:
				g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.lights.Count() - 1;
				break;
			case 2:
				g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.cameras.Count() - 1;
				break;
			}
		}

		// Element pivot change
		if (g_DirectorsCutSystem.elementIndex > -1)
		{
			CModelElement* elementE;
			CLightCustomEffect* elementL;
			C_PointCamera* elementC;
			switch (g_DirectorsCutSystem.elementMode)
			{
			case 0:
				if (g_DirectorsCutSystem.elementIndex > g_DirectorsCutSystem.dags.Count() - 1)
					g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.dags.Count() - 1;
				if (g_DirectorsCutSystem.dags.Count() > 0)
					elementE = g_DirectorsCutSystem.dags[g_DirectorsCutSystem.elementIndex];
				else
					elementE = nullptr;
				if (elementE != nullptr)
				{
					if (ImGui::Button("Selected Model to Pivot (Q)") || ImGui::IsKeyPressed(ImGuiKey_Q, false))
					{
						g_DirectorsCutSystem.pivot = elementE->GetAbsOrigin();
					}

					if (elementE->IsRagdoll())
					{
						if (g_DirectorsCutSystem.boneIndex > -1)
						{
							if (ImGui::Button("Selected Physics Object to Pivot (Z)") || ImGui::IsKeyPressed(ImGuiKey_Z, false))
							{
								QAngle dummy;
								elementE->m_pRagdoll->GetElement(g_DirectorsCutSystem.boneIndex)->GetPosition(&g_DirectorsCutSystem.pivot, &dummy);
							}
						}
					}
					if (g_DirectorsCutSystem.nextPoseIndex > -1)
					{
						if (ImGui::Button("Selected Pose Bone to Pivot (Z)") || ImGui::IsKeyPressed(ImGuiKey_Z, false))
						{
							QAngle dummy;
							g_DirectorsCutSystem.needToSetPoseBoneToPivot = true;
						}
					}
				}
				break;
			case 1:
				if (g_DirectorsCutSystem.elementIndex > g_DirectorsCutSystem.lights.Count() - 1)
					g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.lights.Count() - 1;
				if (g_DirectorsCutSystem.lights.Count() > 0)
					elementL = g_DirectorsCutSystem.lights[g_DirectorsCutSystem.elementIndex];
				else
					elementL = nullptr;
				if (elementL != nullptr)
				{
					if (ImGui::Button("Selected Light to Pivot (Q)") || ImGui::IsKeyPressed(ImGuiKey_Q, false))
					{
						g_DirectorsCutSystem.pivot = elementL->GetAbsOrigin();
					}
				}
				break;
			case 2:
				if (g_DirectorsCutSystem.elementIndex > g_DirectorsCutSystem.cameras.Count() - 1)
					g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.cameras.Count() - 1;
				if (g_DirectorsCutSystem.cameras.Count() > 0)
					elementC = g_DirectorsCutSystem.cameras[g_DirectorsCutSystem.elementIndex];
				else
					elementC = nullptr;
				if (elementC != nullptr)
				{
					if (ImGui::Button("Selected Camera to Pivot (Q)") || ImGui::IsKeyPressed(ImGuiKey_Q, false))
					{
						g_DirectorsCutSystem.pivot = elementC->GetAbsOrigin();
					}
				}
				break;
			}
		}
		
		// Camera adjustments
		bool viewDirty = false;

		// TODO: camera dags should unlock field of view slider and hide gizmos
		//viewDirty |= ImGui::SliderFloat("Field of View", &g_DirectorsCutSystem.fov, 1, 179.0f, "%.0f");
		ImGui::LabelText("Field of View", "%.0f", g_DirectorsCutSystem.fov);

		float oldDistance = g_DirectorsCutSystem.distance;
		float mouseDistance = -io.MouseWheel * cvar_dx_zoom_distance.GetFloat();
		float newDistance = g_DirectorsCutSystem.distance + mouseDistance;
		
		if (newDistance >= 5.f && newDistance <= 1000.f)
			g_DirectorsCutSystem.distance = newDistance;
		else if (newDistance < 5.f)
			g_DirectorsCutSystem.distance = 5.f;
		else if (newDistance > 1000.f)
			g_DirectorsCutSystem.distance = 1000.f;
		
		if (oldDistance != g_DirectorsCutSystem.distance)
			viewDirty = true;
		
		viewDirty |= ImGui::SliderFloat("Distance", &g_DirectorsCutSystem.distance, 5, 1000.0f, "%.0f");
		
		// Camera movement
		if (ImGui::IsMouseDown(2))
		{
			float sensitivity;
			Vector2D mouseDelta = Vector2D(io.MouseDelta.x, io.MouseDelta.y);
			
			/*
			if (ImGui::IsKeyDown(ImGuiKey_LeftShift))
			{
				// FIXME: Panning
				// Shift+middle click to pan pivot
				// Local space translation
				sensitivity = cvar_dx_pan_sensitivity.GetFloat();
				Vector forward, right, up;
				AngleVectors(g_DirectorsCutSystem.engineAngles, &forward, &right, &up);
				Msg("===== ORBIT =====\n");
				Msg("=================\n");
				Msg("forward: %f %f %f\n", forward.x, forward.y, forward.z);
				Msg("right: %f %f %f\n", right.x, right.y, right.z);
				Msg("up: %f %f %f\n", up.x, up.y, up.z);
				Msg("=================\n");
				// Panning up and down relative to angles (viewport)
				// Use forward vector to determine weight
				mouseDelta.x *= forward.x;
				mouseDelta.y *= forward.z;
				Vector pivotDelta = right * mouseDelta.x * sensitivity + up * mouseDelta.y * sensitivity;
				g_DirectorsCutSystem.pivot -= pivotDelta;
			}
			else
			{
			*/
			// Middle click orbit
			sensitivity = cvar_dx_orbit_sensitivity.GetFloat();
			g_DirectorsCutSystem.camYAngle += mouseDelta.x * sensitivity;
			g_DirectorsCutSystem.camXAngle += mouseDelta.y * sensitivity;
			// Clamp to 90 degrees
			if(g_DirectorsCutSystem.camXAngle > 90.f)
				g_DirectorsCutSystem.camXAngle = 90.f;
			else if (g_DirectorsCutSystem.camXAngle < -90.f)
				g_DirectorsCutSystem.camXAngle = -90.f;
			//}
			viewDirty = true;
		}

		// Reset camera view to accommodate new distance
		if (viewDirty || firstFrame)
		{
			float eye[3];
			eye[0] = cosf(g_DirectorsCutSystem.camYAngle) * cosf(g_DirectorsCutSystem.camXAngle) * g_DirectorsCutSystem.distance;
			eye[1] = sinf(g_DirectorsCutSystem.camXAngle) * g_DirectorsCutSystem.distance;
			eye[2] = sinf(g_DirectorsCutSystem.camYAngle) * cosf(g_DirectorsCutSystem.camXAngle) * g_DirectorsCutSystem.distance;
			float at[] = { 0.f, 0.f, 0.f };
			float up[] = { 0.f, 1.f, 0.f };
			g_DirectorsCutSystem.LookAt(eye, at, up, g_DirectorsCutSystem.cameraView);
			firstFrame = false;
		}

		// Grid
		ImGui::Checkbox("Draw Grid", &g_DirectorsCutSystem.drawGrid);
		if (g_DirectorsCutSystem.drawGrid)
		{
			ImGui::InputFloat("Grid Size", &g_DirectorsCutSystem.gridSize);
		}
		
		// Operation (gizmo mode)
		ImGui::SliderInt("Set Operation", &g_DirectorsCutSystem.operation, -1, 2);
		switch (g_DirectorsCutSystem.operation)
		{
			case -1:
				ImGui::LabelText("Operation", "None");
				break;
			case 0:
				ImGui::LabelText("Operation", "Translate");
				mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
				break;
			case 1:
				ImGui::LabelText("Operation", "Rotate");
				mCurrentGizmoOperation = ImGuizmo::ROTATE;
				break;
			case 2:
				ImGui::LabelText("Operation", "Universal");
				mCurrentGizmoOperation = ImGuizmo::TRANSLATE | ImGuizmo::ROTATE;
		}
		if (ImGui::IsKeyPressed(ImGuiKey_N, false))
			g_DirectorsCutSystem.operation = -1;
		if (ImGui::IsKeyPressed(ImGuiKey_T, false))
			g_DirectorsCutSystem.operation = 0;
		if (ImGui::IsKeyPressed(ImGuiKey_R, false))
			g_DirectorsCutSystem.operation = 1;
		if (ImGui::IsKeyPressed(ImGuiKey_Y, false))
			g_DirectorsCutSystem.operation = 2;

		// Snapping
		ImGui::Checkbox("Use Snapping", &g_DirectorsCutSystem.useSnap);
		if (g_DirectorsCutSystem.useSnap)
			ImGui::InputFloat3("Snap", g_DirectorsCutSystem.snap);
		
		ImGui::SliderFloat("Time Scale", &g_DirectorsCutSystem.timeScale, 0.01f, 1.f, "%.2f");

		// Global space toggles
		if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
			mCurrentGizmoMode = ImGuizmo::LOCAL;
		ImGui::SameLine();
		if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
			mCurrentGizmoMode = ImGuizmo::WORLD;
		ImGui::SameLine();
		ImGui::Text("(L)");

		if (ImGui::IsKeyPressed(ImGuiKey_L, false))
		{
			if (mCurrentGizmoMode == ImGuizmo::LOCAL)
				mCurrentGizmoMode = ImGuizmo::WORLD;
			else
				mCurrentGizmoMode = ImGuizmo::LOCAL;
		}


		ImGui::End();

		ImGui::Begin("Elements", 0, ImGuiWindowFlags_AlwaysAutoResize);

		if (!g_DirectorsCutSystem.levelInit)
		{
			ImGui::Text("Level not loaded");
		}
		else
		{
			ImGui::SliderInt("Set Element Mode", &g_DirectorsCutSystem.nextElementMode, -1, 2);
			float modelMatrixPivot[16];
			Vector posPivot = g_DirectorsCutSystem.newPivot;
			QAngle angPivot = QAngle();
			float sPivot = 1.f;
			static float translationPivot[3], rotationPivot[3], scalePivot[3];
			switch (g_DirectorsCutSystem.elementMode)
			{
			case -1:
				ImGui::LabelText("Element Mode", "Pivot");

				if (g_DirectorsCutSystem.operation <= -1)
					break;

				ImGuizmo::SetID(-1);

				// Add offset (pivot) to translation
				posPivot -= g_DirectorsCutSystem.pivot;

				// Y up to Z up - pos is Z up, translation is Y up
				translationPivot[0] = -posPivot.y;
				translationPivot[1] = posPivot.z;
				translationPivot[2] = -posPivot.x;

				rotationPivot[0] = -angPivot.x;
				rotationPivot[1] = angPivot.y;
				rotationPivot[2] = -angPivot.z;

				scalePivot[0] = sPivot;
				scalePivot[1] = sPivot;
				scalePivot[2] = sPivot;

				ImGuizmo::RecomposeMatrixFromComponents(translationPivot, rotationPivot, scalePivot, modelMatrixPivot);
				float deltaMatrix[16];
				float deltaTranslation[3], deltaRotation[3], deltaScale[3];
				ImGuizmo::Manipulate(g_DirectorsCutSystem.cameraView, g_DirectorsCutSystem.cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, modelMatrixPivot, deltaMatrix, (g_DirectorsCutSystem.useSnap ? g_DirectorsCutSystem.snap : NULL), NULL, NULL);
				ImGuizmo::DecomposeMatrixToComponents(deltaMatrix, deltaTranslation, deltaRotation, deltaScale);
				// Revert Z up
				float translation2[3];
				translation2[0] = -deltaTranslation[2];
				translation2[1] = -deltaTranslation[0];
				translation2[2] = deltaTranslation[1];
				float rotation2[3];
				rotation2[0] = -deltaRotation[0];
				rotation2[1] = deltaRotation[1];
				rotation2[2] = -deltaRotation[2];
				if (ImGuizmo::IsUsing())
				{
					ImGui::LabelText("Delta Translation", "%f %f %f", deltaTranslation[0], deltaTranslation[1], deltaTranslation[2]);
					ImGui::LabelText("Delta Rotation", "%f %f %f", deltaRotation[0], deltaRotation[1], deltaRotation[2]);
					// Create source vars from new values
					Vector newPos = Vector(0, 0, 0);
					QAngle newAng = QAngle(rotation2[0], rotation2[1], rotation2[2]);
					if (newAng.x == 0.f && newAng.y == 0.f && newAng.z == 0.f && newAng.x == -0.f && newAng.y == -0.f && newAng.z == -0.f)
						newPos = Vector(translation2[0], translation2[1], translation2[2]);
					g_DirectorsCutSystem.newPivot += newPos;
					g_DirectorsCutSystem.justSetPivot = true;

				}
				else if (g_DirectorsCutSystem.justSetPivot)
				{
					g_DirectorsCutSystem.pivot = g_DirectorsCutSystem.newPivot;
					g_DirectorsCutSystem.justSetPivot = false;
				}
				else
				{
					g_DirectorsCutSystem.newPivot = g_DirectorsCutSystem.pivot;
				}
				break;
			case 0:
				ImGui::LabelText("Element Mode", "Models");

				ImGui::InputText("Model Name", &g_DirectorsCutSystem.modelName);
				
				if (ImGui::Button("Spawn Model"))
					CreateDag((char*)g_DirectorsCutSystem.modelName.c_str(), true, true);
				ImGui::SameLine();
				if (ImGui::Button("Spawn Model at Pivot"))
				{
					CModelElement* pEntity = CreateDag((char*)g_DirectorsCutSystem.modelName.c_str(), true, true);
					if (pEntity != nullptr)
					{
						pEntity->SetAbsOrigin(g_DirectorsCutSystem.pivot);
					}
				}

				if (g_DirectorsCutSystem.dags.Count() > 0)
				{
					// failsafe
					if (g_DirectorsCutSystem.elementIndex < 0)
						g_DirectorsCutSystem.elementIndex = 0;
					ImGui::LabelText("Models", "%d", g_DirectorsCutSystem.dags.Count());
					if (g_DirectorsCutSystem.dags.Count() > 1)
					{
						if (ImGui::SliderInt("Model Index", &g_DirectorsCutSystem.elementIndex, 0, g_DirectorsCutSystem.dags.Count() - 1))
						{
							CModelElement* pEntity = g_DirectorsCutSystem.dags[g_DirectorsCutSystem.elementIndex];
							if (pEntity != nullptr && pEntity->IsRagdoll() && g_DirectorsCutSystem.ragdollIndex != g_DirectorsCutSystem.elementIndex)
								g_DirectorsCutSystem.ragdollIndex = g_DirectorsCutSystem.elementIndex;
							g_DirectorsCutSystem.boneIndex = -1;
							g_DirectorsCutSystem.nextPoseIndex = -1;
							g_DirectorsCutSystem.flexIndex = -1;
						}
					}
					else
					{
						ImGui::LabelText("Model Index", "%d", g_DirectorsCutSystem.elementIndex);
					}
					CModelElement* pEntity = g_DirectorsCutSystem.dags[g_DirectorsCutSystem.elementIndex];
					if (pEntity != nullptr)
					{
						ImGui::LabelText("Selected Model Name", "%s", pEntity->GetModelPtr()->pszName());
						CStudioHdr* modelPtr = pEntity->GetModelPtr();
						if (modelPtr)
						{
							if (pEntity->IsRagdoll())
							{
								for (int i = 0; i < modelPtr->numbones(); i++)
								{
									mstudiobone_t* bone = modelPtr->pBone(i);
									if (bone != nullptr)
									{
										if (bone->physicsbone == g_DirectorsCutSystem.boneIndex)
										{
											char* boneName = bone->pszName();
											ImGui::LabelText("Physics Object Name", "%s", boneName);
											break;
										}
									}
								}
								ImGui::SliderInt("Physics Object Index", &g_DirectorsCutSystem.boneIndex, -1, pEntity->m_pRagdoll->RagdollBoneCount() - 1);
								CRagdoll* ragdoll = pEntity->m_pRagdoll;
								if (ragdoll != nullptr && g_DirectorsCutSystem.boneIndex >= 0)
								{
									IPhysicsObject* bone = ragdoll->GetElement(g_DirectorsCutSystem.boneIndex);
									if (bone != nullptr)
									{
										// F key also freezes
										if (!bone->IsMotionEnabled())
										{
											if (ImGui::Button("Unfreeze Physics Object (F)") || ImGui::IsKeyPressed(ImGuiKey_F, false))
											{
												PhysForceClearVelocity(bone);
												bone->EnableMotion(true);
												bone->Wake();
												PhysForceClearVelocity(bone);
											}
										}
										else
										{
											if (ImGui::Button("Freeze Physics Object (F)") || ImGui::IsKeyPressed(ImGuiKey_F, false))
											{
												PhysForceClearVelocity(bone);
												bone->Sleep();
												bone->EnableMotion(false);
												PhysForceClearVelocity(bone);
											}
										}
									}
								}
								if (ImGui::Button("Freeze All"))
								{
									for (int i = 0; i < ragdoll->RagdollBoneCount(); i++)
									{
										IPhysicsObject* bone = ragdoll->GetElement(i);
										if (bone != nullptr)
										{
											PhysForceClearVelocity(bone);
											bone->Sleep();
											bone->EnableMotion(false);
											PhysForceClearVelocity(bone);
										}
									}
								}
								ImGui::SameLine();
								if (ImGui::Button("Unfreeze All"))
								{
									for (int i = 0; i < ragdoll->RagdollBoneCount(); i++)
									{
										IPhysicsObject* bone = ragdoll->GetElement(i);
										if (bone != nullptr)
										{
											PhysForceClearVelocity(bone);
											bone->EnableMotion(true);
											bone->Wake();
											PhysForceClearVelocity(bone);
										}
									}
								}
							}
							else if (modelinfo->GetVCollide(pEntity->GetModelIndex()) != nullptr)
							{
								if (ImGui::Button("Make Physical"))
								{
									g_DirectorsCutSystem.dags.Remove(g_DirectorsCutSystem.elementIndex);
									g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.dags.Count() - 1;
									g_DirectorsCutSystem.boneIndex = -1;
									MakeRagdoll(pEntity, true, true);
								}
							}
							for (int i = 0; i < modelPtr->numbones(); i++)
							{
								mstudiobone_t* bone = modelPtr->pBone(i);
								if (bone != nullptr)
								{
									if (i == g_DirectorsCutSystem.nextPoseIndex)
									{
										char* boneName = bone->pszName();
										ImGui::LabelText("Pose Bone Name", "%s", boneName);
										break;
									}
								}
							}
							ImGui::SliderInt("Pose Bone Index", &g_DirectorsCutSystem.nextPoseIndex, -1, modelPtr->numbones() - 1);
							for (int i = 0; i < (int)modelPtr->numflexcontrollers(); i++)
							{
								if (i == g_DirectorsCutSystem.flexIndex)
								{
									mstudioflexcontroller_t* pflexcontroller = modelPtr->pFlexcontroller((LocalFlexController_t)i);
									char* flexName = (char*)pEntity->GetFlexControllerName((LocalFlexController_t)i);
									if (pflexcontroller->max != pflexcontroller->min)
									{
										float flex = pEntity->GetFlexWeight((LocalFlexController_t)i);
										ImGui::SliderFloat(flexName, &flex, pflexcontroller->min, pflexcontroller->max);
										pEntity->SetFlexWeight((LocalFlexController_t)i, flex);
									}
									else
									{
										ImGui::LabelText("Flex", "%s", flexName);
									}
									break;
								}
							}
							ImGui::SliderInt("Flex Index", &g_DirectorsCutSystem.flexIndex, -1, modelPtr->numflexcontrollers() - 1);
							if (ImGui::Button("View Target to Camera"))
							{
								modelrender->SetViewTarget(modelPtr, pEntity->GetBody(), g_DirectorsCutSystem.engineOrigin);
							}
						}
					}
					if (ImGui::Button("Remove Model"))
					{
						g_DirectorsCutSystem.dags.Remove(g_DirectorsCutSystem.elementIndex);
						g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.dags.Count() - 1;
						g_DirectorsCutSystem.boneIndex = -1;
						g_DirectorsCutSystem.nextPoseIndex = -1;
						g_DirectorsCutSystem.flexIndex = -1;
						pEntity->Remove();
					}
					if (g_DirectorsCutSystem.elementIndex > -1 && g_DirectorsCutSystem.operation > -1)
					{
						// Retry for pEntity pointer in case we just removed it
						pEntity = g_DirectorsCutSystem.dags[g_DirectorsCutSystem.elementIndex];
						if (pEntity != nullptr)
						{
							ImGuizmo::SetID(g_DirectorsCutSystem.elementIndex);

							float modelMatrix[16];
							Vector pos = pEntity->GetAbsOrigin();
							QAngle ang = pEntity->GetAbsAngles();
							float s = pEntity->GetModelScale();
							if (pEntity->IsRagdoll() && g_DirectorsCutSystem.boneIndex >= 0)
							{
								pEntity->m_pRagdoll->GetElement(g_DirectorsCutSystem.boneIndex)->GetPosition(&pos, &ang);
							}
							else if (g_DirectorsCutSystem.nextPoseIndex >= 0)
							{
								pos = g_DirectorsCutSystem.poseBoneOrigin;
								ang = g_DirectorsCutSystem.poseBoneAngles;
							}

							static float translation[3], rotation[3], scale[3];

							// Add offset (pivot) to translation
							pos -= g_DirectorsCutSystem.pivot;

							// Y up to Z up - pos is Z up, translation is Y up
							translation[0] = -pos.y;
							translation[1] = pos.z;
							translation[2] = -pos.x;

							rotation[0] = -ang.x;
							rotation[1] = ang.y;
							rotation[2] = -ang.z;
							
							scale[0] = s;
							scale[1] = s;
							scale[2] = s;

							ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, modelMatrix);
							float deltaMatrix[16];
							float deltaTranslation[3], deltaRotation[3], deltaScale[3];
							ImGuizmo::Manipulate(g_DirectorsCutSystem.cameraView, g_DirectorsCutSystem.cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, modelMatrix, deltaMatrix, (g_DirectorsCutSystem.useSnap ? g_DirectorsCutSystem.snap : NULL), NULL, NULL);
							ImGuizmo::DecomposeMatrixToComponents(deltaMatrix, deltaTranslation, deltaRotation, deltaScale);
							// Revert Z up
							float translation2[3];
							translation2[0] = -deltaTranslation[2];
							translation2[1] = -deltaTranslation[0];
							translation2[2] = deltaTranslation[1];
							float rotation2[3];
							rotation2[0] = -deltaRotation[0];
							rotation2[1] = deltaRotation[1];
							rotation2[2] = -deltaRotation[2];
							if (ImGuizmo::IsUsing())
							{
								ImGui::LabelText("Delta Translation", "%f %f %f", deltaTranslation[0], deltaTranslation[1], deltaTranslation[2]);
								ImGui::LabelText("Delta Rotation", "%f %f %f", deltaRotation[0], deltaRotation[1], deltaRotation[2]);
								// Create source vars from new values
								Vector newPos = Vector(0, 0, 0);
								QAngle newAng = QAngle(rotation2[0], rotation2[1], rotation2[2]);
								if (newAng.x == 0.f && newAng.y == 0.f && newAng.z == 0.f && newAng.x == -0.f && newAng.y == -0.f && newAng.z == -0.f)
									newPos = Vector(translation2[0], translation2[1], translation2[2]);
								g_DirectorsCutSystem.deltaOrigin = newPos;
								g_DirectorsCutSystem.deltaAngles = newAng;
							}
							else
							{
								g_DirectorsCutSystem.deltaOrigin = Vector(0, 0, 0);
								g_DirectorsCutSystem.deltaAngles = QAngle(0, 0, 0);
							}
						}
					}
				}
				else
				{
					ImGui::Text("No models found");
				}
				break;
			case 1:
				ImGui::LabelText("Element Mode", "Lights");
				
				//ImGui::InputText("Light Texture", &g_DirectorsCutSystem.lightTexture);

				if (ImGui::Button("Spawn Light"))
					CreateLight((char*)g_DirectorsCutSystem.lightTexture.c_str(), true, true);
				ImGui::SameLine();
				if (ImGui::Button("Spawn Light at Pivot"))
				{
					CLightCustomEffect* pEntity = CreateLight((char*)g_DirectorsCutSystem.lightTexture.c_str(), true, true);
					if (pEntity != nullptr)
					{
						pEntity->SetAbsOrigin(g_DirectorsCutSystem.pivot);
					}
				}

				if (g_DirectorsCutSystem.lights.Count() > 0)
				{
					// failsafe
					if (g_DirectorsCutSystem.elementIndex < 0)
						g_DirectorsCutSystem.elementIndex = 0;
					ImGui::LabelText("Lights", "%d", g_DirectorsCutSystem.lights.Count());
					if (g_DirectorsCutSystem.lights.Count() > 1)
					{
						ImGui::SliderInt("Light Index", &g_DirectorsCutSystem.elementIndex, 0, g_DirectorsCutSystem.lights.Count() - 1);
					}
					else
					{
						ImGui::LabelText("Light Index", "%d", g_DirectorsCutSystem.elementIndex);
					}
					CLightCustomEffect* pEntity = g_DirectorsCutSystem.lights[g_DirectorsCutSystem.elementIndex];
					//if (pEntity != nullptr)
						//ImGui::LabelText("Selected Light Texture", "%s", pEntity->GetSpotlightTextureName());
					if (ImGui::Button("Remove Light"))
					{
						g_DirectorsCutSystem.lights.Remove(g_DirectorsCutSystem.elementIndex);
						g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.lights.Count() - 1;
						g_DirectorsCutSystem.boneIndex = -1;
						pEntity->Remove();
					}
					if (g_DirectorsCutSystem.elementIndex > -1 && g_DirectorsCutSystem.operation > -1)
					{
						// Retry for pEntity pointer in case we just removed it
						pEntity = g_DirectorsCutSystem.lights[g_DirectorsCutSystem.elementIndex];
						if (pEntity != nullptr)
						{
							ImGuizmo::SetID(g_DirectorsCutSystem.elementIndex);

							float modelMatrix[16];
							Vector pos = pEntity->GetAbsOrigin();
							QAngle ang = pEntity->GetAbsAngles();
							float s = 1.f;

							static float translation[3], rotation[3], scale[3];

							// Add offset (pivot) to translation
							pos -= g_DirectorsCutSystem.pivot;

							// Y up to Z up - pos is Z up, translation is Y up
							translation[0] = -pos.y;
							translation[1] = pos.z;
							translation[2] = -pos.x;

							rotation[0] = -ang.x;
							rotation[1] = ang.y;
							rotation[2] = -ang.z;

							scale[0] = s;
							scale[1] = s;
							scale[2] = s;

							ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, modelMatrix);
							float deltaMatrix[16];
							float deltaTranslation[3], deltaRotation[3], deltaScale[3];
							ImGuizmo::Manipulate(g_DirectorsCutSystem.cameraView, g_DirectorsCutSystem.cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, modelMatrix, deltaMatrix, (g_DirectorsCutSystem.useSnap ? g_DirectorsCutSystem.snap : NULL), NULL, NULL);
							ImGuizmo::DecomposeMatrixToComponents(deltaMatrix, deltaTranslation, deltaRotation, deltaScale);
							// Revert Z up
							float translation2[3];
							translation2[0] = -deltaTranslation[2];
							translation2[1] = -deltaTranslation[0];
							translation2[2] = deltaTranslation[1];
							float rotation2[3];
							rotation2[0] = -deltaRotation[0];
							rotation2[1] = deltaRotation[1];
							rotation2[2] = -deltaRotation[2];
							if (ImGuizmo::IsUsing())
							{
								ImGui::LabelText("Delta Translation", "%f %f %f", deltaTranslation[0], deltaTranslation[1], deltaTranslation[2]);
								ImGui::LabelText("Delta Rotation", "%f %f %f", deltaRotation[0], deltaRotation[1], deltaRotation[2]);
								// Create source vars from new values
								Vector newPos = Vector(0, 0, 0);
								QAngle newAng = QAngle(rotation2[0], rotation2[1], rotation2[2]);
								if (newAng.x == 0.f && newAng.y == 0.f && newAng.z == 0.f && newAng.x == -0.f && newAng.y == -0.f && newAng.z == -0.f)
									newPos = Vector(translation2[0], translation2[1], translation2[2]);
								g_DirectorsCutSystem.deltaOrigin = newPos;
								g_DirectorsCutSystem.deltaAngles = newAng;
							}
							else
							{
								g_DirectorsCutSystem.deltaOrigin = Vector(0, 0, 0);
								g_DirectorsCutSystem.deltaAngles = QAngle(0, 0, 0);
							}
						}
					}
				}
				else
				{
					ImGui::Text("No lights found");
				}

				break;
			case 2:
				ImGui::LabelText("Element Mode", "Cameras");

				if (ImGui::Button("Spawn Camera"))
					CreateCamera(true, true);
				ImGui::SameLine();
				if (ImGui::Button("Spawn Camera at Pivot"))
				{
					C_PointCamera* pEntity = CreateCamera(true, true);
					if (pEntity != nullptr)
					{
						pEntity->SetAbsOrigin(g_DirectorsCutSystem.pivot);
					}
				}

				if (g_DirectorsCutSystem.cameras.Count() > 0)
				{
					// failsafe
					if (g_DirectorsCutSystem.elementIndex < 0)
						g_DirectorsCutSystem.elementIndex = 0;
					ImGui::LabelText("Cameras", "%d", g_DirectorsCutSystem.cameras.Count());
					if (g_DirectorsCutSystem.cameras.Count() > 1)
					{
						ImGui::SliderInt("Camera Index", &g_DirectorsCutSystem.elementIndex, 0, g_DirectorsCutSystem.cameras.Count() - 1);
					}
					else
					{
						ImGui::LabelText("Camera Index", "%d", g_DirectorsCutSystem.elementIndex);
					}
					C_PointCamera* pEntity = g_DirectorsCutSystem.cameras[g_DirectorsCutSystem.elementIndex];
					if (ImGui::Button("Remove Camera"))
					{
						g_DirectorsCutSystem.cameras.Remove(g_DirectorsCutSystem.elementIndex);
						g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.cameras.Count() - 1;
						g_DirectorsCutSystem.boneIndex = -1;
						pEntity->Remove();
					}
					if (g_DirectorsCutSystem.elementIndex > -1 && g_DirectorsCutSystem.operation > -1)
					{
						// Retry for pEntity pointer in case we just removed it
						pEntity = g_DirectorsCutSystem.cameras[g_DirectorsCutSystem.elementIndex];
						if (pEntity != nullptr)
						{
							ImGuizmo::SetID(g_DirectorsCutSystem.elementIndex);

							float modelMatrix[16];
							Vector pos = pEntity->GetAbsOrigin();
							QAngle ang = pEntity->GetAbsAngles();
							float s = 1.f;

							static float translation[3], rotation[3], scale[3];

							// Add offset (pivot) to translation
							pos -= g_DirectorsCutSystem.pivot;

							// Y up to Z up - pos is Z up, translation is Y up
							translation[0] = -pos.y;
							translation[1] = pos.z;
							translation[2] = -pos.x;

							rotation[0] = -ang.x;
							rotation[1] = ang.y;
							rotation[2] = -ang.z;

							scale[0] = s;
							scale[1] = s;
							scale[2] = s;

							ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, modelMatrix);
							float deltaMatrix[16];
							float deltaTranslation[3], deltaRotation[3], deltaScale[3];
							ImGuizmo::Manipulate(g_DirectorsCutSystem.cameraView, g_DirectorsCutSystem.cameraProjection, mCurrentGizmoOperation, mCurrentGizmoMode, modelMatrix, deltaMatrix, (g_DirectorsCutSystem.useSnap ? g_DirectorsCutSystem.snap : NULL), NULL, NULL);
							ImGuizmo::DecomposeMatrixToComponents(deltaMatrix, deltaTranslation, deltaRotation, deltaScale);
							// Revert Z up
							float translation2[3];
							translation2[0] = -deltaTranslation[2];
							translation2[1] = -deltaTranslation[0];
							translation2[2] = deltaTranslation[1];
							float rotation2[3];
							rotation2[0] = -deltaRotation[0];
							rotation2[1] = deltaRotation[1];
							rotation2[2] = -deltaRotation[2];
							if (ImGuizmo::IsUsing())
							{
								ImGui::LabelText("Delta Translation", "%f %f %f", deltaTranslation[0], deltaTranslation[1], deltaTranslation[2]);
								ImGui::LabelText("Delta Rotation", "%f %f %f", deltaRotation[0], deltaRotation[1], deltaRotation[2]);
								// Create source vars from new values
								Vector newPos = Vector(0, 0, 0);
								QAngle newAng = QAngle(rotation2[0], rotation2[1], rotation2[2]);
								if (newAng.x == 0.f && newAng.y == 0.f && newAng.z == 0.f && newAng.x == -0.f && newAng.y == -0.f && newAng.z == -0.f)
									newPos = Vector(translation2[0], translation2[1], translation2[2]);
								g_DirectorsCutSystem.deltaOrigin = newPos;
								g_DirectorsCutSystem.deltaAngles = newAng;
							}
							else
							{
								g_DirectorsCutSystem.deltaOrigin = Vector(0, 0, 0);
								g_DirectorsCutSystem.deltaAngles = QAngle(0, 0, 0);
							}
						}
					}
				}
				else
				{
					ImGui::Text("No cameras found");
				}
				
				break;
			}
		}
		
		ImGui::End();

		ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
		ImGuizmo::SetRect(0, 0, windowWidth, windowHeight);

		if (g_DirectorsCutSystem.drawGrid)
			ImGuizmo::DrawGrid(g_DirectorsCutSystem.cameraView, g_DirectorsCutSystem.cameraProjection, g_DirectorsCutSystem.identityMatrix, g_DirectorsCutSystem.gridSize);

		// Obsolete due to middle mouse orbiting
		//ImGuizmo::ViewManipulate(g_DirectorsCutSystem.cameraView, g_DirectorsCutSystem.distance, ImVec2(windowWidth - 192, 0), ImVec2(192, 192), 0x10101010);

		// info box in bottom corner
		ImGui::SetNextWindowPos(ImVec2(windowWidth - 264 - 8, windowHeight - 96 - 8), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(264, 96), ImGuiCond_Always);
		ImGui::Begin("Info", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoInputs);

		ImGui::Text("Director's Cut");
		ImGui::Text("Version: %s", g_DirectorsCutSystem.directorcut_version.GetVersion());
		ImGui::Text("Author: %s", "KiwifruitDev");
		ImGui::Text("License: %s", "MIT");
		ImGui::Text("https://twitter.com/SFMDirectorsCut");
	
		ImGui::End();

		ImGui::EndFrame();

		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
	}
	trampEndScene(p_pDevice);
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT WINAPI WndProc(const HWND hWnd, const UINT msg, const WPARAM wParam, const LPARAM lParam)
{
	if (cvar_imgui_enabled.GetBool() && cvar_imgui_input_enabled.GetBool() && !engine->IsPaused())
	{
		ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
	}
	return CallWindowProc(ogWndProc, hWnd, msg, wParam, lParam);
}

void CDirectorsCutSystem::SetupEngineView(Vector& origin, QAngle& angles, float& fov)
{
	if (levelInit && cvar_imgui_input_enabled.GetBool())
	{
		fov = this->fov;
		matrix_t_dx cameraDxMatrix = *(matrix_t_dx*)cameraView;
		matrix3x4_t cameraVMatrix;

		// Convert ImGuizmo matrix to matrix3x4_t
		for (int i = 0; i < 3; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				cameraVMatrix[i][j] = cameraDxMatrix.m[i][j];
			}
		}

		// Apply matrix to angles
		MatrixTranspose(cameraVMatrix, cameraVMatrix);
		MatrixAngles(cameraVMatrix, engineAngles);

		// TODO: Fix bad matrix angles
		if (engineAngles.y == 180)
		{
			angles.x = engineAngles.z;
			angles.y = -engineAngles.x;
			angles.z = engineAngles.y;
		}
		else
		{
			angles.x = -engineAngles.z;
			angles.y = engineAngles.x;
			angles.z = -engineAngles.y;
		}

		// Rotate origin around pivot point with distance
		Vector pivot = this->pivot;
		float distance = this->distance;
		Vector forward, right, up;
		AngleVectors(angles, &forward, &right, &up);
		engineOrigin = pivot - (forward * distance);
		origin = engineOrigin;
	}
}

void CDirectorsCutSystem::PostInit()
{
	float newCameraMatrix[16] =
	{ 1.f, 0.f, 0.f, 0.f,
	  0.f, 1.f, 0.f, 0.f,
	  0.f, 0.f, 1.f, 0.f,
	  0.f, 0.f, 0.f, 1.f };
	for (int i = 0; i < 16; i++)
	{
		cameraView[i] = newCameraMatrix[i];
	}
	float newIdentityMatrix[16] =
	{ 1.f, 0.f, 0.f, 0.f,
		0.f, 1.f, 0.f, 0.f,
		0.f, 0.f, 1.f, 0.f,
		0.f, 0.f, 0.f, 1.f };
	for (int i = 0; i < 16; i++)
	{
		identityMatrix[i] = newIdentityMatrix[i];
	}
	float newSnap[3] = { 1.f, 1.f, 1.f };
	for (int i = 0; i < 3; i++)
	{
		snap[i] = newSnap[i];
	}
	// Hook Direct3D in hl2.exe
	Msg("Director's Cut: Hooking Direct3D...\n");
	HWND hwnd = FindWindowA("Valve001", NULL);
	CDirect3DHook direct3DHook = GetDirect3DHook();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hwnd);
	imguiActive = true;
	char* endSceneChar = reinterpret_cast<char*>(&EndScene);
	int hooked = direct3DHook.Hook(hwnd, endSceneChar);
	switch (hooked)
	{
		case 0: // No error
			ogWndProc = (WNDPROC)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);
			Msg("Director's Cut: System loaded\n");
			break;
		case 1: // Already hooked
			Msg("Director's Cut: System already hooked\n");
			break;
		case 2: // Unable to hook endscene
			Msg("Director's Cut: Unable to hook endscene\n");
			break;
		case 4: // Invalid HWND
			Msg("Director's Cut: System failed to load - Invalid HWND\n");
			break;
		case 5: // Invalid device table
			Msg("Director's Cut: System failed to load - Invalid device table\n");
			break;
		case 6: // Could not find Direct3D system
			Msg("Director's Cut: System failed to load - Could not find Direct3D system\n");
			break;
		case 7: // Failed to create Direct3D device
			Msg("Director's Cut: System failed to load - Failed to create Direct3D device\n");
			break;
		case 3: // Future error
		default: // Unknown error
			Msg("Director's Cut: System failed to load - Unknown error\n");
			break;
	}
	// Unload Dear ImGui on nonzero
	if (hooked != 0)
	{
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		imguiActive = false;
	}
}

void CDirectorsCutSystem::Shutdown()
{
	if (imguiActive)
	{
		// Unload Dear ImGui
		ImGui_ImplDX9_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		imguiActive = false;
	}
	// Unhook
	CDirect3DHook direct3DHook = GetDirect3DHook();
	direct3DHook.Unhook();
	LevelShutdownPreEntity();
}

void CDirectorsCutSystem::LevelInitPostEntity()
{
	levelInit = true;
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (pPlayer != nullptr)
	{
		// noclip
		if (pPlayer->GetMoveType() != MOVETYPE_NOCLIP)
		{
			// send noclip command to server
			engine->ClientCmd_Unrestricted("noclip");
		}
	}
}

void CDirectorsCutSystem::LevelShutdownPreEntity()
{
	levelInit = false;
	// loop g_DirectorsCutSystem.dags to remove all entities
	for (int i = 0; i < g_DirectorsCutSystem.dags.Count() - 1; i++)
	{
		g_DirectorsCutSystem.dags[i]->Remove();
	}
	// loop g_DirectorsCutSystem.lights to remove all lights
	for (int i = 0; i < g_DirectorsCutSystem.lights.Count() - 1; i++)
	{
		g_DirectorsCutSystem.lights[i]->Remove();
	}
	// loop g_DirectorsCutSystem.cameras to remove all cameras
	for (int i = 0; i < g_DirectorsCutSystem.cameras.Count() - 1; i++)
	{
		g_DirectorsCutSystem.cameras[i]->Remove();
	}
	// clear list
	g_DirectorsCutSystem.dags.RemoveAll();
}

void CDirectorsCutSystem::Update(float frametime)
{
	bool newState = (cvar_imgui_enabled.GetBool() && cvar_imgui_input_enabled.GetBool() && !engine->IsPaused());
	if (cursorState != newState)
	{
		cursorState = newState;
		if (newState)
		{
			vgui::surface()->SetCursorAlwaysVisible(true);
			vgui::surface()->UnlockCursor();
		}
		else
		{
			vgui::surface()->SetCursorAlwaysVisible(false);
			vgui::surface()->LockCursor();
			int w, h;
			engine->GetScreenSize(w, h);
			int x = w >> 1;
			int y = h >> 1;
			vgui::surface()->SurfaceSetCursorPos(x, y);
		}
	}
	C_BasePlayer* pPlayer = C_BasePlayer::GetLocalPlayer();
	if (pPlayer != nullptr)
	{
		clienttools->GetLocalPlayerEyePosition(playerOrigin, playerAngles, playerFov);
	}
	if (timeScale != currentTimeScale)
	{
		ConVar* varTimeScale = cvar->FindVar("host_timescale");
		varTimeScale->SetValue(timeScale);
		currentTimeScale = timeScale;
	}
	Vector newPos = deltaOrigin;
	QAngle newAng = deltaAngles;
	if (elementIndex > -1)
	{
		CModelElement* pEntity;
		CLightCustomEffect* pLight;
		C_PointCamera* pCamera;
		switch (elementMode)
		{
		case 0:
			if (elementIndex > dags.Count())
				break;
			pEntity = dags[elementIndex];
			if (pEntity != nullptr)
			{
				if (pEntity->IsRagdoll() && boneIndex >= 0)
				{
					if (newAng.x != 0.f || newAng.y != 0.f || newAng.z != 0.f || newPos.x != 0.f || newPos.y != 0.f || newPos.z != 0.f)
					{
						IPhysicsObject* bone = pEntity->m_pRagdoll->GetElement(boneIndex);
						if (bone != nullptr)
						{
							PhysForceClearVelocity(bone);
							bone->Wake();
							// apply delta to bone
							Vector origin;
							QAngle angles;
							bone->GetPosition(&origin, &angles);
							angles += deltaAngles;
							origin += deltaOrigin;
							bone->SetPosition(origin, angles, true);
						}
					}
				}
				else if(nextPoseIndex >= 0)
				{
					int nextPoseIndexTrue = nextPoseIndex;
					// pretend that the bone is in world space (it isn't)
					pEntity->PushAllowBoneAccess(true, true, "DirectorsCut");
					Vector worldVec;
					QAngle worldAng;
					MatrixAngles(pEntity->GetBoneForWrite(nextPoseIndexTrue), worldAng, worldVec);
					poseBoneOrigin = worldVec + pEntity->posadds[nextPoseIndexTrue];
					poseBoneAngles = worldAng + pEntity->anglehelper[nextPoseIndexTrue];
					if (newAng.x != 0.f || newAng.y != 0.f || newAng.z != 0.f || newPos.x != 0.f || newPos.y != 0.f || newPos.z != 0.f)
					{
						// apply delta to pose
						poseBoneAngles += deltaAngles;
						poseBoneOrigin += deltaOrigin;
						// remove world space for final pose
						Vector localVec = poseBoneOrigin - worldVec;
						QAngle localAng = poseBoneAngles - worldAng;
						pEntity->PoseBones(nextPoseIndexTrue, localVec, localAng);
					}
					if (needToSetPoseBoneToPivot)
					{
						pivot = poseBoneOrigin;
						needToSetPoseBoneToPivot = false;
					}
					pEntity->PopBoneAccess("DirectorsCut");
				}
				else
				{
					if (newAng.x != 0.f || newAng.y != 0.f || newAng.z != 0.f || newPos.x != 0.f || newPos.y != 0.f || newPos.z != 0.f)
					{
						// apply delta to entity
						Vector origin = pEntity->GetAbsOrigin();
						QAngle angles = pEntity->GetAbsAngles();
						angles += deltaAngles;
						origin += deltaOrigin;
						pEntity->SetAbsOrigin(origin);
						pEntity->SetAbsAngles(angles);
					}
				}
			}
			break;
		case 1:
			if (elementIndex > lights.Count())
				break;
			pLight = lights[elementIndex];
			if (pLight != nullptr)
			{
				// apply delta to entity
				Vector origin = pLight->GetAbsOrigin();
				QAngle angles = pLight->GetAbsAngles();
				angles += deltaAngles;
				origin += deltaOrigin;
				pLight->SetAbsOrigin(origin);
				pLight->SetAbsAngles(angles);
			}
			break;
		case 2:
			if (elementIndex > cameras.Count())
				break;
			pCamera = cameras[elementIndex];
			if (pCamera != nullptr)
			{
				// apply delta to entity
				Vector origin = pCamera->GetAbsOrigin();
				QAngle angles = pCamera->GetAbsAngles();
				angles += deltaAngles;
				origin += deltaOrigin;
				pCamera->SetAbsOrigin(origin);
				pCamera->SetAbsAngles(angles);
			}
			break;
		}
	}
	for (int i = 0; i < lights.Count(); i++)
	{
		CLightCustomEffect* light = lights[i];
		if (light != nullptr)
		{
			// get dir, right, and up from angles
			QAngle angles = light->GetAbsAngles();
			Vector dir, right, up;
			AngleVectors(angles, &dir, &right, &up);
			light->UpdateLight(light->GetAbsOrigin(), dir, right, up, 1000);
		}
	}

	if (levelInit && cvar_imgui_enabled.GetBool() && cvar_imgui_input_enabled.GetBool() && !engine->IsPaused())
	{
		if (selecting)
		{
			if (operation != -1)
			{
				operation = oldOperation;
				operation = -1;
			}
			// holding down CTRL (selecting) should allow the user to select individual objects
			float vector = 0.5f;
			Vector boundMin = Vector(-vector, -vector, -vector);
			Vector boundMax = Vector(vector, vector, vector);
			int colorR = 196;
			int colorG = 107;
			int colorB = 174;
			int colorA = 255;
			int colorR_hover = 255;
			int colorG_hover = 255;
			int colorB_hover = 255;
			int colorA_hover = 255;
			int colorR_selected = 0;
			int colorG_selected = 255;
			int colorB_selected = 255;
			int colorA_selected = 255;
			int mouseX, mouseY;
			input->GetFullscreenMousePos(&mouseX, &mouseY);
			if (elementIndex <= -1)
				return;
			int modelsPhysOrBones = 0;
			if (boneIndex >= 0)
				modelsPhysOrBones = 1;
			if (nextPoseIndex >= 0)
				modelsPhysOrBones = 2;
			bool clicked = ImGui::IsMouseClicked(0);
			switch (elementMode)
			{
			case 0:
				if (modelsPhysOrBones == 0)
				{
					// loop all models
					for (int i = 0; i < dags.Count(); i++)
					{
						// draw model locations and names
						CModelElement* model = dags[i];
						char modelNameAndIndex[MAXCHAR];
						sprintf(modelNameAndIndex, "%d (%s)", i, model->GetModelName());
						if(i == elementIndex)
						{
							//NDebugOverlay::Box(model->GetAbsOrigin(), boundMin, boundMax, colorR_selected, colorG_selected, colorB_selected, colorA_selected, length);
							NDebugOverlay::EntityTextAtPosition(model->GetAbsOrigin(), 0, modelNameAndIndex, frametime, colorR_selected, colorG_selected, colorB_selected, colorA_selected);
						}
						else
						{
							// is mouse in 3d bounds
							Vector screenPos;
							ScreenTransform(model->GetAbsOrigin(), screenPos);
							// vector is normalized from -1 to 1, so we have to normalize mouseX and mouseY
							float mouseXNorm = (mouseX / (float)ScreenWidth()) * 2 - 1;
							float mouseYNorm = ((mouseY / (float)ScreenHeight()) * 2 - 1) * -1;
							// instead of directly computing boundMin and boundMax in 3d space, we'll use vector as an estimate
							if (mouseXNorm > screenPos.x - (vector * 0.1) && mouseXNorm < screenPos.x + (vector * 0.1) && mouseYNorm > screenPos.y - (vector * 0.1) && mouseYNorm < screenPos.y + (vector * 0.1))
							{
								//NDebugOverlay::Box(model->GetAbsOrigin(), boundMin, boundMax, colorR_hover, colorG_hover, colorB_hover, colorA_hover, length);
								NDebugOverlay::EntityTextAtPosition(model->GetAbsOrigin(), 0, modelNameAndIndex, frametime, colorR_hover, colorG_hover, colorB_hover, colorA_hover);
								// forced to use imgui as it swallows mouse input
								if (clicked)
								{
									vgui::surface()->PlaySound("common/wpn_select.wav");
									// select model
									clicked = false;
									g_DirectorsCutSystem.deltaOrigin = Vector(0, 0, 0);
									g_DirectorsCutSystem.deltaAngles = QAngle(0, 0, 0);
									elementIndex = i;
								}
							}
							else
							{
								//NDebugOverlay::Box(model->GetAbsOrigin(), boundMin, boundMax, colorR, colorG, colorB, colorA, length);
								NDebugOverlay::EntityTextAtPosition(model->GetAbsOrigin(), 0, modelNameAndIndex, frametime, colorR, colorG, colorB, colorA);
							}
						}
					}
				}
				else
				{
					if (elementIndex > dags.Count())
						break;
					// use the current model
					CModelElement* pEntity = dags[elementIndex];
					CStudioHdr* modelPtr = pEntity->GetModelPtr();
					if (modelPtr != nullptr)
					{
						if (pEntity != nullptr)
						{
							if (modelsPhysOrBones == 1)
							{
								// loop all physics objects
								CRagdoll* pRagdoll = pEntity->m_pRagdoll;
								if (pRagdoll != nullptr)
								{
									for (int i = 0; i < pRagdoll->RagdollBoneCount(); i++)
									{
										IPhysicsObject* pPhys = pRagdoll->GetElement(i);
										if (pPhys != nullptr)
										{
											// draw physics object locations and names
											for (int j = 0; j < modelPtr->numbones(); j++)
											{
												mstudiobone_t* bone = modelPtr->pBone(j);
												if (bone != nullptr)
												{
													if (bone->physicsbone == i)
													{
														Vector physPos;
														QAngle physAngles;
														pPhys->GetPosition(&physPos, &physAngles);
														char nameAndIndex[MAXCHAR];
														sprintf(nameAndIndex, "%d", i);
														if(i == boneIndex)
														{
															//NDebugOverlay::Box(physPos, boundMin, boundMax, colorR_selected, colorG_selected, colorB_selected, colorA_selected, length);
															NDebugOverlay::EntityTextAtPosition(physPos, 0, nameAndIndex, frametime, colorR_selected, colorG_selected, colorB_selected, colorA_selected);
														}
														else
														{
															// is mouse in 3d bounds
															Vector screenPos;
															ScreenTransform(physPos, screenPos);
															// vector is normalized from -1 to 1, so we have to normalize mouseX and mouseY
															float mouseXNorm = (mouseX / (float)ScreenWidth()) * 2 - 1;
															float mouseYNorm = ((mouseY / (float)ScreenHeight()) * 2 - 1) * -1;
															// instead of directly computing boundMin and boundMax in 3d space, we'll use vector as an estimate
															if (mouseXNorm > screenPos.x - (vector * 0.1) && mouseXNorm < screenPos.x + (vector * 0.1) && mouseYNorm > screenPos.y - (vector * 0.1) && mouseYNorm < screenPos.y + (vector * 0.1))
															{
																//NDebugOverlay::Box(physPos, boundMin, boundMax, colorR_hover, colorG_hover, colorB_hover, colorA_hover, length);
																NDebugOverlay::EntityTextAtPosition(physPos, 0, nameAndIndex, frametime, colorR_hover, colorG_hover, colorB_hover, colorA_hover);
																// forced to use imgui as it swallows mouse input
																if (clicked)
																{
																	vgui::surface()->PlaySound("common/wpn_select.wav");
																	// select model
																	clicked = false;
																	g_DirectorsCutSystem.deltaOrigin = Vector(0, 0, 0);
																	g_DirectorsCutSystem.deltaAngles = QAngle(0, 0, 0);
																	boneIndex = i;
																}
															}
															else
															{
																//NDebugOverlay::Box(physPos, boundMin, boundMax, colorR, colorG, colorB, colorA, length);
																NDebugOverlay::EntityTextAtPosition(physPos, 0, nameAndIndex, frametime, colorR, colorG, colorB, colorA);
															}
														}
														break;
													}
												}
											}
										}
									}
								}
							}
							else if (modelsPhysOrBones == 2)
							{
								pEntity->PushAllowBoneAccess(true, true, "DirectorsCut_PostRender");
								// loop all pose bones
								for (int i = 0; i < modelPtr->numbones(); i++)
								{
									mstudiobone_t* bone = modelPtr->pBone(i);
									if (bone != nullptr)
									{
										// draw pose bone locations and names
										Vector worldVec;
										QAngle worldAng;
										MatrixAngles(pEntity->GetBoneForWrite(i), worldAng, worldVec);
										char nameAndIndex[MAXCHAR];
										sprintf(nameAndIndex, "%d", i); //bone->pszName());
										//Msg("%s (len: %d)\n", nameAndIndex, strlen(nameAndIndex));
										// is mouse in 3d bounds
										Vector screenPos;
										ScreenTransform(worldVec + pEntity->posadds[i], screenPos);
										// vector is normalized from -1 to 1, so we have to normalize mouseX and mouseY
										float mouseXNorm = (mouseX / (float)ScreenWidth()) * 2 - 1;
										float mouseYNorm = ((mouseY / (float)ScreenHeight()) * 2 - 1) * -1;
										// instead of directly computing boundMin and boundMax in 3d space, we'll use vector as an estimate
										if(i == nextPoseIndex)
										{
											//NDebugOverlay::Box(worldVec + pEntity->posadds[i], boundMin, boundMax, colorR_selected, colorG_selected, colorB_selected, colorA_selected, length);
											NDebugOverlay::EntityTextAtPosition(worldVec + pEntity->posadds[i], 0, nameAndIndex, frametime, colorR_selected, colorG_selected, colorB_selected, colorA_selected);
										}
										else
										{
											if (mouseXNorm > screenPos.x - (vector * 0.1) && mouseXNorm < screenPos.x + (vector * 0.1) && mouseYNorm > screenPos.y - (vector * 0.1) && mouseYNorm < screenPos.y + (vector * 0.1))
											{
												//NDebugOverlay::Box(worldVec + pEntity->posadds[i], boundMin, boundMax, colorR_hover, colorG_hover, colorB_hover, colorA_hover, length);
												NDebugOverlay::EntityTextAtPosition(worldVec + pEntity->posadds[i], 0, nameAndIndex, frametime, colorR_hover, colorG_hover, colorB_hover, colorA_hover);
												// forced to use imgui as it swallows mouse input
												if (clicked)
												{
													vgui::surface()->PlaySound("common/wpn_select.wav");
													// select model
													clicked = false;
													g_DirectorsCutSystem.deltaOrigin = Vector(0, 0, 0);
													g_DirectorsCutSystem.deltaAngles = QAngle(0, 0, 0);
													nextPoseIndex = i;
												}
											}
											else
											{
												//NDebugOverlay::Box(worldVec + pEntity->posadds[i], boundMin, boundMax, colorR, colorG, colorB, colorA, length);
												NDebugOverlay::EntityTextAtPosition(worldVec + pEntity->posadds[i], 0, nameAndIndex, frametime, colorR, colorG, colorB, colorA);
											}
										}
									}
								}
								pEntity->PopBoneAccess("DirectorsCut_PostRender");
							}
						}
					}
				}
				break;
			case 1:
				// loop lights
				for (int i = 0; i < lights.Count(); i++)
				{
					// draw light locations and names
					CLightCustomEffect* light = lights[i];
					char lightNameAndIndex[MAXCHAR];
					sprintf(lightNameAndIndex, "%d", i);
					if(i == elementIndex)
					{
						//NDebugOverlay::Box(light->GetAbsOrigin(), boundMin, boundMax, colorR_selected, colorG_selected, colorB_selected, colorA_selected, length);
						NDebugOverlay::EntityTextAtPosition(light->GetAbsOrigin(), 0, lightNameAndIndex, frametime, colorR_selected, colorG_selected, colorB_selected, colorA_selected);
					}
					else
					{
						// is mouse in 3d bounds
						Vector screenPos;
						ScreenTransform(light->GetAbsOrigin(), screenPos);
						// vector is normalized from -1 to 1, so we have to normalize mouseX and mouseY
						float mouseXNorm = (mouseX / (float)ScreenWidth()) * 2 - 1;
						float mouseYNorm = ((mouseY / (float)ScreenHeight()) * 2 - 1) * -1;
						// instead of directly computing boundMin and boundMax in 3d space, we'll use vector as an estimate
						if (mouseXNorm > screenPos.x - (vector * 0.1) && mouseXNorm < screenPos.x + (vector * 0.1) && mouseYNorm > screenPos.y - (vector * 0.1) && mouseYNorm < screenPos.y + (vector * 0.1))
						{
							//NDebugOverlay::Box(light->GetAbsOrigin(), boundMin, boundMax, colorR_hover, colorG_hover, colorB_hover, colorA_hover, length);
							NDebugOverlay::EntityTextAtPosition(light->GetAbsOrigin(), 0, lightNameAndIndex, frametime, colorR_hover, colorG_hover, colorB_hover, colorA_hover);
							// forced to use imgui as it swallows mouse input
							if (clicked)
							{
								vgui::surface()->PlaySound("common/wpn_select.wav");
								// select model
								clicked = false;
								g_DirectorsCutSystem.deltaOrigin = Vector(0, 0, 0);
								g_DirectorsCutSystem.deltaAngles = QAngle(0, 0, 0);
								elementIndex = i;
							}
						}
						else
						{
							//NDebugOverlay::Box(light->GetAbsOrigin(), boundMin, boundMax, colorR, colorG, colorB, colorA, length);
							NDebugOverlay::EntityTextAtPosition(light->GetAbsOrigin(), 0, lightNameAndIndex, frametime, colorR, colorG, colorB, colorA);
						}
					}
				}
				break;
			case 2:
				// loop cameras
				for (int i = 0; i < cameras.Count(); i++)
				{
					// draw camera locations and names
					C_PointCamera* camera = cameras[i];
					char cameraNameAndIndex[MAXCHAR];
					sprintf(cameraNameAndIndex, "%d", i);
					if(i == elementIndex)
					{
						//NDebugOverlay::Box(camera->GetAbsOrigin(), boundMin, boundMax, colorR_selected, colorG_selected, colorB_selected, colorA_selected, length);
						NDebugOverlay::EntityTextAtPosition(camera->GetAbsOrigin(), 0, cameraNameAndIndex, frametime, colorR_selected, colorG_selected, colorB_selected, colorA_selected);
					}
					else
					{
						// is mouse in 3d bounds
						Vector screenPos;
						ScreenTransform(camera->GetAbsOrigin(), screenPos);
						// vector is normalized from -1 to 1, so we have to normalize mouseX and mouseY
						float mouseXNorm = (mouseX / (float)ScreenWidth()) * 2 - 1;
						float mouseYNorm = ((mouseY / (float)ScreenHeight()) * 2 - 1) * -1;
						// instead of directly computing boundMin and boundMax in 3d space, we'll use vector as an estimate
						if (mouseXNorm > screenPos.x - (vector * 0.1) && mouseXNorm < screenPos.x + (vector * 0.1) && mouseYNorm > screenPos.y - (vector * 0.1) && mouseYNorm < screenPos.y + (vector * 0.1))
						{
							//NDebugOverlay::Box(camera->GetAbsOrigin(), boundMin, boundMax, colorR_hover, colorG_hover, colorB_hover, colorA_hover, length);
							NDebugOverlay::EntityTextAtPosition(camera->GetAbsOrigin(), 0, cameraNameAndIndex, frametime, colorR_hover, colorG_hover, colorB_hover, colorA_hover);
							// forced to use imgui as it swallows mouse input
							if (clicked)
							{
								vgui::surface()->PlaySound("common/wpn_select.wav");
								// select model
								clicked = false;
								g_DirectorsCutSystem.deltaOrigin = Vector(0, 0, 0);
								g_DirectorsCutSystem.deltaAngles = QAngle(0, 0, 0);
								elementIndex = i;
							}
						}
						else
						{
							//NDebugOverlay::Box(camera->GetAbsOrigin(), boundMin, boundMax, colorR, colorG, colorB, colorA, length);
							NDebugOverlay::EntityTextAtPosition(camera->GetAbsOrigin(), 0, cameraNameAndIndex, frametime, colorR, colorG, colorB, colorA);
						}
					}
				}
				break;
			}
		}
		else if (operation != oldOperation)
		{
			operation = oldOperation;
		}
		input->ClearInputButton(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT | IN_JUMP | IN_DUCK | IN_ATTACK | IN_ATTACK2 | IN_RELOAD | IN_USE | IN_CANCEL | IN_LEFT | IN_RIGHT | IN_ATTACK3 | IN_SCORE | IN_SPEED | IN_WALK | IN_ZOOM | IN_BULLRUSH | IN_RUN);
		// set pose bone index
		// TODO: make sure bone is in range
		if (poseIndex != nextPoseIndex && nextPoseIndex >= 0 && elementIndex >= 0 && elementIndex < dags.Count())
		{
			CModelElement* pEntity = dags[elementIndex];
			pEntity->GetBoneForWrite(nextPoseIndex);
			poseIndex = nextPoseIndex;
		}
	}
}

CModelElement* CreateDag(char* modelName, bool add, bool setIndex)
{
	// Prepend "models/" if not present
	if (strncmp(modelName, "models/", 7) != 0)
	{
		char newModelName[256];
		sprintf(newModelName, "models/%s", modelName);
		modelName = newModelName;
	}
	// Append ".mdl" if not present
	if (strstr(modelName, ".mdl") == NULL)
	{
		char newModelName[256];
		sprintf(newModelName, "%s.mdl", modelName);
		modelName = newModelName;
	}
	
	// Cache model
	model_t* model = (model_t*)engine->LoadModel(modelName);
	if (!model)
	{
		Msg("Director's Cut: Failed to load model %s\n", modelName);
		return nullptr;
	}
	INetworkStringTable* precacheTable = networkstringtable->FindTable("modelprecache");
	if (precacheTable)
	{
		modelinfo->FindOrLoadModel(modelName);
		int idx = precacheTable->AddString(false, modelName);
		if (idx == INVALID_STRING_INDEX)
		{
			Msg("Director's Cut: Failed to precache model %s\n", modelName);
		}
	}
	
	// Create test dag
	CModelElement* pEntity = new CModelElement();
	if (!pEntity)
		return nullptr;
	pEntity->SetModel(modelName);
	pEntity->SetModelName(modelName);
	
	// Spawn entity
	RenderGroup_t renderGroup = RENDER_GROUP_OPAQUE_ENTITY;
	if (!pEntity->InitializeAsClientEntity(modelName, renderGroup))
	{
		Msg("Director's Cut: Failed to spawn entity %s\n", modelName);
		pEntity->Release();
		return nullptr;
	}

	// Add entity to list
	if (add)
	{
		g_DirectorsCutSystem.dags.AddToTail(pEntity);
		if(setIndex)
			g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.dags.Count() - 1;
	}
	Msg("Director's Cut: Created dag with model name %s and render group %d\n", modelName, renderGroup);
	return pEntity;
}

CLightCustomEffect* CreateLight(char* texture, bool add, bool setIndex)
{
	CLightCustomEffect* pEntity = new CLightCustomEffect();
	if (!pEntity)
	{
		Msg("Director's Cut: Failed to create light with texture %s\n", texture);
		return nullptr;
	}
	/*
	CTextureReference textureReference;
	textureReference.Init(materials->FindTexture(texture, TEXTURE_GROUP_OTHER));
	pEntity->SetSpotlightTextureName(texture);
	pEntity->SetTextureReference(textureReference);
	pEntity->SetForceUpdate(true);
	pEntity->SetLightFov(90.f);
	pEntity->SetLightHorizontalFov(90.f);
	pEntity->SetShadowQuality(1);
	pEntity->SetLightWorld(true);
	pEntity->SetNearZ(4.f);
	pEntity->SetFarZ(750.f);
	pEntity->SetState(true);
	*/
	// Add entity to list
	if (add)
	{
		g_DirectorsCutSystem.lights.AddToTail(pEntity);
		if (setIndex)
			g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.lights.Count() - 1;
	}
	Msg("Director's Cut: Created light with texture name %s\n", texture);
	return pEntity;
}

C_PointCamera* CreateCamera(bool add, bool setIndex)
{
	C_PointCamera* pEntity = new C_PointCamera();
	if (!pEntity)
	{
		Msg("Director's Cut: Failed to create camera\n");
		return nullptr;
	}
	// Add entity to list
	if (add)
	{
		g_DirectorsCutSystem.cameras.AddToTail(pEntity);
		if (setIndex)
			g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.cameras.Count() - 1;
	}
	Msg("Director's Cut: Created camera\n");
	return pEntity;
}

CModelElement* MakeRagdoll(CModelElement* dag)
{
	if (modelinfo->GetVCollide(dag->GetModelIndex()) == nullptr)
	{
		Msg("Director's Cut: Model %s has no vcollide\n", dag->GetModelName());
		return nullptr;
	}
	CStudioHdr* modelPtr = dag->GetModelPtr();
	if (modelPtr)
	{
		CModelElement* ragdoll = dag->BecomeRagdollOnClient();
		if (ragdoll != nullptr)
		{
			IPhysicsObject* pPhys = dag->VPhysicsGetObject();
			if (pPhys)
			{
				pPhys->SetPosition(dag->GetAbsOrigin(), dag->GetAbsAngles(), true);
			}
			// Freeze all bones
			CRagdoll* cRagdoll = ragdoll->m_pRagdoll;
			if (cRagdoll != nullptr)
			{
				dag->PushAllowBoneAccess(true, true, "DirectorsCut_FindChildren");
				ragdoll->GetBoneForWrite(0); // update bone allow state
				for (int i = 0; i < cRagdoll->RagdollBoneCount(); i++)
				{
					IPhysicsObject* bone = cRagdoll->GetElement(i);
					if (bone != nullptr)
					{
						PhysForceClearVelocity(bone);
						bone->EnableMotion(false);
					}
				}
				dag->PopBoneAccess("DirectorsCut_FindChildren");
			}
			return ragdoll;
		}
	}
	return nullptr;
}

CModelElement* MakeRagdoll(CModelElement* dag, bool add, bool setIndex)
{
	CModelElement* ragdoll = MakeRagdoll(dag);
	if (ragdoll != nullptr)
	{
		// Add entity to list
		if (add)
		{
			g_DirectorsCutSystem.dags.AddToTail(ragdoll);
			if (setIndex)
			{
				g_DirectorsCutSystem.elementIndex = g_DirectorsCutSystem.dags.Count() - 1;
				g_DirectorsCutSystem.ragdollIndex = g_DirectorsCutSystem.dags.Count() - 1;
				g_DirectorsCutSystem.nextPoseIndex = -1;
				g_DirectorsCutSystem.boneIndex = -1;
				g_DirectorsCutSystem.flexIndex = -1;
			}
		}
		return ragdoll;
	}
	return nullptr;
}

CModelElement* CreateRagdoll(char* modelName, bool add, bool setIndex)
{
	// Init ragdoll
	CModelElement* pEntity = CreateDag(modelName, false, false);
	if (pEntity != nullptr)
	{
		CModelElement* ragdoll = MakeRagdoll(pEntity, add, setIndex);
		if (ragdoll != nullptr)
		{
			return ragdoll;
		}
	}
	return nullptr;
}

void CDirectorsCutSystem::Frustum(float left, float right, float bottom, float top, float znear, float zfar, float* m16)
{
	float temp, temp2, temp3, temp4;
	temp = 2.0f * znear;
	temp2 = right - left;
	temp3 = top - bottom;
	temp4 = zfar - znear;
	m16[0] = temp / temp2;
	m16[1] = 0.0;
	m16[2] = 0.0;
	m16[3] = 0.0;
	m16[4] = 0.0;
	m16[5] = temp / temp3;
	m16[6] = 0.0;
	m16[7] = 0.0;
	m16[8] = (right + left) / temp2;
	m16[9] = (top + bottom) / temp3;
	m16[10] = (-zfar - znear) / temp4;
	m16[11] = -1.0f;
	m16[12] = 0.0;
	m16[13] = 0.0;
	m16[14] = (-temp * zfar) / temp4;
	m16[15] = 0.0;
}

void CDirectorsCutSystem::OrthoGraphic(const float l, float r, float b, const float t, float zn, const float zf, float* m16)
{
	m16[0] = 2 / (r - l);
	m16[1] = 0.0f;
	m16[2] = 0.0f;
	m16[3] = 0.0f;
	m16[4] = 0.0f;
	m16[5] = 2 / (t - b);
	m16[6] = 0.0f;
	m16[7] = 0.0f;
	m16[8] = 0.0f;
	m16[9] = 0.0f;
	m16[10] = 1.0f / (zf - zn);
	m16[11] = 0.0f;
	m16[12] = (l + r) / (l - r);
	m16[13] = (t + b) / (b - t);
	m16[14] = zn / (zn - zf);
	m16[15] = 1.0f;
}

void CDirectorsCutSystem::Perspective(float fovyInDegrees, float aspectRatio, float znear, float zfar, float* m16)
{
	float ymax, xmax;
	ymax = znear * tanf(fovyInDegrees * 3.141592f / 180.0f);
	xmax = ymax * aspectRatio;
	this->Frustum(-xmax, xmax, -ymax, ymax, znear, zfar, m16);
}

void Cross(const float* a, const float* b, float* r)
{
	r[0] = a[1] * b[2] - a[2] * b[1];
	r[1] = a[2] * b[0] - a[0] * b[2];
	r[2] = a[0] * b[1] - a[1] * b[0];
}

float Dot(const float* a, const float* b)
{
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Normalize(const float* a, float* r)
{
	float il = 1.f / (sqrtf(Dot(a, a)) + FLT_EPSILON);
	r[0] = a[0] * il;
	r[1] = a[1] * il;
	r[2] = a[2] * il;
}

void CDirectorsCutSystem::LookAt(const float* eye, const float* at, const float* up, float* m16)
{
	float X[3], Y[3], Z[3], tmp[3];

	tmp[0] = eye[0] - at[0];
	tmp[1] = eye[1] - at[1];
	tmp[2] = eye[2] - at[2];
	Normalize(tmp, Z);
	Normalize(up, Y);

	Cross(Y, Z, tmp);
	Normalize(tmp, X);

	Cross(Z, X, tmp);
	Normalize(tmp, Y);

	m16[0] = X[0];
	m16[1] = Y[0];
	m16[2] = Z[0];
	m16[3] = 0.0f;
	m16[4] = X[1];
	m16[5] = Y[1];
	m16[6] = Z[1];
	m16[7] = 0.0f;
	m16[8] = X[2];
	m16[9] = Y[2];
	m16[10] = Z[2];
	m16[11] = 0.0f;
	m16[12] = -Dot(X, eye);
	m16[13] = -Dot(Y, eye);
	m16[14] = -Dot(Z, eye);
	m16[15] = 1.0f;
}

