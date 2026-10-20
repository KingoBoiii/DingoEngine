#include "CrashTrigger.h"

#include <DingoEngine/Assertion.h>
#include <DingoEngine/Asset/AssetManager.h>
#include <DingoEngine/Core/Application.h>
#include <DingoEngine/Core/CrashHandler.h>
#include <DingoEngine/Core/FatalError.h>
#include <DingoEngine/Physics/3D/Physics3D.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <thread>
#include <vector>

namespace Dingo
{

	namespace
	{
		void WriteThroughNull()
		{
			volatile int* volatile pointer = nullptr;
			*pointer = 1;
		}

		// Bounded so no compiler sees it as infinite, but far deeper than any stack.
		uint64_t Recurse(uint64_t depth)
		{
			volatile char frame[512];
			frame[0] = static_cast<char>(depth);
			const uint64_t below = depth < (uint64_t(1) << 40) ? Recurse(depth + 1) : depth;
			return below + static_cast<uint64_t>(frame[0]);
		}

		void CrashThreadsTogether()
		{
			constexpr int k_Threads = 8;
			std::atomic<int> ready = 0;
			std::atomic<bool> go = false;
			std::vector<std::thread> threads;
			for (int i = 0; i < k_Threads; i++)
			{
				threads.emplace_back([&]
				{
					ready++;
					while (!go)
					{
					}
					WriteThroughNull();
				});
			}
			while (ready < k_Threads)
				std::this_thread::yield();
			go = true;
			for (std::thread& thread : threads)
				thread.join();
		}

		void ThrowRuntimeError()
		{
			throw std::runtime_error("the test app's engine-thread throw");
		}

		void RunOnEngineThread(void (*body)())
		{
			std::thread([&body]
			{
				Internal::CrashHandler::RunThread([](void* context) { (*static_cast<void (**)()>(context))(); }, &body);
			}).join();
		}

		void RunOnPreparedThread(void (*body)())
		{
			std::thread([body]
			{
				Internal::CrashHandler::PrepareThread();
				body();
			}).join();
		}

		void CrashByRecursing()
		{
			DE_WARN("[Crash] {}", Recurse(0));
		}

		void CrashOnAssetWorker(Internal::CrashHandler::ProbeFunction body)
		{
			Internal::CrashHandler::ArmProbe(Internal::CrashHandler::ProbeThread::AssetWorker, body);
			AssetManager& assets = Application::Get().GetAssetManager();
			if (assets.GetState(assets.LoadAsync("textures/hd2.png")) != AssetState::Loading)
			{
				Internal::CrashHandler::TakeProbe(Internal::CrashHandler::ProbeThread::AssetWorker);
				DE_WARN("[Crash] --crash=asset-throw queued no decode job; run it on a case that hasn't loaded textures/hd2.png");
			}
		}

		// The world has to outlive the job Step queues, and deleting it would run the job on this
		// thread, so this waits for the crash and never deletes it.
		void CrashOnPhysicsWorker(Internal::CrashHandler::ProbeFunction body)
		{
			Physics3D* physics = Physics3D::Create();
			physics->Initialize(Physics3DParams());
			Internal::CrashHandler::ArmProbe(Internal::CrashHandler::ProbeThread::PhysicsWorker, body);
			physics->Step(1.0f / 60.0f);
			std::this_thread::sleep_for(std::chrono::seconds(10));
			FatalError("No Jolt worker crashed within 10 s.");
		}

		[[noreturn]] void TerminateWithException()
		{
			try
			{
				throw std::runtime_error("the test app's --crash=terminate");
			}
			catch (...)
			{
				std::terminate();
			}
		}
	}

	void TriggerCrash(std::string_view kind)
	{
		DE_WARN("[Crash] --crash={}: crashing on purpose", kind);
		if (Application::Get().IsHeadless() && (kind.starts_with("render-") || kind == "asset-throw"))
			DE_WARN("[Crash] --crash={} has no render thread or asset decode to crash when headless", kind);
		if (kind == "segv")
			WriteThroughNull();
		else if (kind == "thread")
			std::thread(WriteThroughNull).join();
		else if (kind == "engine-thread")
			RunOnEngineThread(WriteThroughNull);
		else if (kind == "engine-throw")
			RunOnEngineThread(ThrowRuntimeError);
		else if (kind == "engine-stack")
			RunOnEngineThread(CrashByRecursing);
		else if (kind == "prepared-throw")
			RunOnPreparedThread(ThrowRuntimeError);
		else if (kind == "prepared-stack")
			RunOnPreparedThread(CrashByRecursing);
		else if (kind == "render-throw")
			Internal::CrashHandler::ArmProbe(Internal::CrashHandler::ProbeThread::Render, ThrowRuntimeError);
		else if (kind == "render-stack")
			Internal::CrashHandler::ArmProbe(Internal::CrashHandler::ProbeThread::Render, CrashByRecursing);
		else if (kind == "asset-throw")
			CrashOnAssetWorker(ThrowRuntimeError);
		else if (kind == "physics-stack")
			CrashOnPhysicsWorker(CrashByRecursing);
		else if (kind == "threads")
			CrashThreadsTogether();
		else if (kind == "segv-after-info")
		{
			DE_INFO("[Crash] last info line");
			WriteThroughNull();
		}
		else if (kind == "abort")
			std::abort();
		else if (kind == "terminate")
			TerminateWithException();
		else if (kind == "throw")
			throw std::runtime_error("the test app's --crash=throw");
		else if (kind == "stack")
			CrashByRecursing();
		else if (kind == "verify")
		{
			DE_VERIFY(false, "the test app's --crash=verify");
		}
		else if (kind == "fatal")
			FatalError("The test app's --crash=fatal.");
		else
			DE_WARN("[Crash] unknown --crash={}; expected segv, thread, engine-thread, engine-throw, engine-stack, prepared-throw, prepared-stack, render-throw, render-stack, asset-throw, physics-stack, threads, segv-after-info, abort, terminate, throw, stack, verify or fatal", kind);
	}

}
