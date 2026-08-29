#include "GameObjectOwnership.h"

#include <iostream>

namespace
{
	class TrackedGameObject final
	{
	public:
		TrackedGameObject() { ++live; }
		~TrackedGameObject()
		{
			game_object_ownership::ReleaseHierarchy(m_pParent, m_pChild, m_pSibling);
			--live;
			++destroyed;
		}

		void AddRef() noexcept { _references.Add(); }
		void Release() noexcept
		{
			if (_references.Release()) delete this;
		}
		void SetChild(TrackedGameObject* child, bool addReference = false) noexcept
		{
			game_object_ownership::AttachChild(*this, child, addReference);
		}

		static void Reset() noexcept
		{
			live = 0;
			destroyed = 0;
		}

		inline static int live = 0;
		inline static int destroyed = 0;

		TrackedGameObject* m_pParent = nullptr;
		TrackedGameObject* m_pChild = nullptr;
		TrackedGameObject* m_pSibling = nullptr;

	private:
		game_object_ownership::ReferenceCount _references;
	};

	bool Expect(bool condition, const char* message)
	{
		if (!condition) std::cerr << "FAIL: " << message << '\n';
		return condition;
	}

	bool TestOwnerReleasesHierarchy()
	{
		TrackedGameObject::Reset();
		{
			game_object_ownership::Owner<TrackedGameObject> root{new TrackedGameObject};
			root->SetChild(new TrackedGameObject);
			root->SetChild(new TrackedGameObject);
		}

		return Expect(TrackedGameObject::live == 0, "GameObjectOwner leaked a hierarchy") &&
			Expect(TrackedGameObject::destroyed == 3, "hierarchy nodes were not destroyed exactly once");
	}

	bool TestSharedChildIsParentlessAndDiesAfterLastOwner()
	{
		TrackedGameObject::Reset();
		auto* model = new TrackedGameObject;
		model->SetChild(new TrackedGameObject);

		auto* first = new TrackedGameObject;
		auto* second = new TrackedGameObject;
		first->SetChild(model, true);
		second->SetChild(model, true);
		bool ok = Expect(model->m_pParent == nullptr,
			"shared child retained one arbitrary parent");

		second->Release();
		ok = Expect(ok, "shared child parent policy failed") &&
			Expect(TrackedGameObject::live == 3, "shared child died before its last owner") &&
			Expect(model->m_pParent == nullptr, "released owner left a dangling parent");
		first->Release();

		return Expect(ok, "shared child lifetime failed") &&
			Expect(TrackedGameObject::live == 0, "shared hierarchy leaked") &&
			Expect(TrackedGameObject::destroyed == 4, "shared hierarchy was destroyed more than once");
	}

	bool TestStructuralChildKeepsParent()
	{
		TrackedGameObject::Reset();
		{
			game_object_ownership::Owner<TrackedGameObject> root{new TrackedGameObject};
			auto* child = new TrackedGameObject;
			root->SetChild(child);
			if (!Expect(child->m_pParent == root.get(), "structural child lost its parent")) return false;
		}

		return Expect(TrackedGameObject::live == 0, "structural hierarchy leaked");
	}

	bool TestAddRefDoesNotWalkHierarchy()
	{
		TrackedGameObject::Reset();
		auto* root = new TrackedGameObject;
		auto* child = new TrackedGameObject;
		root->SetChild(child);

		root->AddRef();
		root->AddRef();
		root->m_pChild = nullptr; // Transfer the child claim to this test.
		child->Release();

		bool ok = Expect(TrackedGameObject::destroyed == 1, "AddRef propagated into the child hierarchy");
		root->Release();
		root->Release();

		return Expect(ok, "AddRef must affect only the target object") &&
			Expect(TrackedGameObject::live == 0, "detached hierarchy leaked");
	}

	bool TestNullChildDoesNotDetachHierarchy()
	{
		TrackedGameObject::Reset();
		{
			game_object_ownership::Owner<TrackedGameObject> root{new TrackedGameObject};
			root->SetChild(new TrackedGameObject);
			root->SetChild(new TrackedGameObject);
			root->SetChild(nullptr);
		}

		return Expect(TrackedGameObject::live == 0, "SetChild(nullptr) detached and leaked a sibling") &&
			Expect(TrackedGameObject::destroyed == 3, "null child changed the hierarchy");
	}
}

int main()
{
	const bool passed = TestOwnerReleasesHierarchy() &&
		TestSharedChildIsParentlessAndDiesAfterLastOwner() &&
		TestStructuralChildKeepsParent() &&
		TestAddRefDoesNotWalkHierarchy() &&
		TestNullChildDoesNotDetachHierarchy();

	if (passed) std::cout << "GameObject ownership tests passed\n";
	return passed ? 0 : 1;
}
