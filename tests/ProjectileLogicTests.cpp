#include "../AvoidTheBoss/TracerVisualState.h"
#include "../AvoidTheBoss/GeneratorState.h"

#ifdef NDEBUG
#undef NDEBUG
#endif

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <new>

namespace
{
std::size_t g_allocationCount = 0;

[[nodiscard]] bool Near(float lhs, float rhs) noexcept
{
	return std::fabs(lhs - rhs) < 0.0001f;
}

void AssertVector(atb::client::TracerVector3 actual,
	atb::client::TracerVector3 expected) noexcept
{
	assert(Near(actual.x, expected.x));
	assert(Near(actual.y, expected.y));
	assert(Near(actual.z, expected.z));
}
}

void* operator new(std::size_t size)
{
	++g_allocationCount;
	if (void* memory = std::malloc(size))
	{
		return memory;
	}
	throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept
{
	std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept
{
	std::free(memory);
}

int main()
{
	using atb::client::TracerVector3;
	using atb::client::TracerVisualState;

	static_assert(TracerVisualState::Capacity == 1);

	GeneratorState generator;
	generator.index = 2;
	assert(generator.IsAvailable());
	assert(generator.BeginInteraction(false));
	assert(!generator.IsAvailable());
	assert(generator.IsAnimating());
	assert(generator.Tick(20.0f) == GeneratorTransition::None);
	assert(Near(generator.progress, 0.0f));
	assert(generator.EndInteraction());

	assert(generator.BeginInteraction(true));
	assert(generator.Tick(4.0f) == GeneratorTransition::None);
	assert(Near(generator.progress, 40.0f));
	assert(generator.EndInteraction());
	assert(generator.BeginInteraction(true));
	assert(generator.Tick(6.0f) == GeneratorTransition::Activated);
	assert(generator.IsActivated());
	assert(Near(generator.progress, generator.maxProgress));
	assert(generator.Tick(1.0f) == GeneratorTransition::None);
	assert(!generator.Activate());

	generator.Reset();
	assert(generator.index == 2);
	assert(generator.IsAvailable());
	assert(!generator.advancesProgress);
	assert(Near(generator.progress, 0.0f));

	TracerVisualState tracer{2.0f, 5.0f};
	assert(tracer.Spawn({1.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 10.0f}));
	assert(tracer.IsActive());
	AssertVector(tracer.Origin(), {1.0f, 2.0f, 3.0f});
	AssertVector(tracer.Position(), {1.0f, 2.0f, 3.0f});
	AssertVector(tracer.Direction(), {0.0f, 0.0f, 1.0f});

	assert(!tracer.Tick(1.0f));
	AssertVector(tracer.Position(), {1.0f, 2.0f, 5.0f});
	assert(Near(tracer.TravelledDistance(), 2.0f));
	assert(tracer.IsActive());

	assert(tracer.Tick(10.0f));
	AssertVector(tracer.Position(), {1.0f, 2.0f, 8.0f});
	assert(Near(tracer.TravelledDistance(), 5.0f));
	assert(!tracer.IsActive());
	assert(!tracer.Tick(1.0f));

	tracer.Reset();
	assert(!tracer.IsActive());
	AssertVector(tracer.Origin(), {});
	AssertVector(tracer.Position(), {});
	AssertVector(tracer.Direction(), {});
	assert(Near(tracer.TravelledDistance(), 0.0f));

	// Local and remote attacks share the same spawn path; no player pointer is retained.
	assert(tracer.Spawn({10.0f, 0.0f, 0.0f}, {-2.0f, 0.0f, 0.0f}));
	AssertVector(tracer.Direction(), {-1.0f, 0.0f, 0.0f});
	tracer.Reset();
	assert(tracer.Spawn({-10.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}));
	AssertVector(tracer.Direction(), {1.0f, 0.0f, 0.0f});

	tracer.Reset();
	assert(!tracer.Spawn({}, {}));
	assert(!tracer.IsActive());

	const std::size_t allocationsBeforeReuse = g_allocationCount;
	for (int i = 0; i < 1'000; ++i)
	{
		assert(tracer.Spawn({}, {0.0f, 1.0f, 0.0f}));
		assert(tracer.Tick(3.0f));
		assert(!tracer.IsActive());
		tracer.Reset();
	}
	assert(g_allocationCount == allocationsBeforeReuse);
}
