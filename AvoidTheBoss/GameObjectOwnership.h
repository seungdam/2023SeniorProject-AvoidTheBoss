#pragma once

#include <cassert>
#include <memory>

namespace game_object_ownership
{
	class ReferenceCount final
	{
	public:
		void Add() noexcept
		{
			assert(_value >= 0);
			++_value;
		}

		[[nodiscard]] bool Release() noexcept
		{
			assert(_value >= 0);
			if (_value == 0) return true; // Legacy adoption: zero still owns one lifetime claim.
			return --_value == 0;
		}

		[[nodiscard]] bool Empty() const noexcept { return _value == 0; }

	private:
		int _value = 0;
	};

	template <typename T>
	void AttachChild(T& owner, T* child, bool addReference) noexcept
	{
		if (!child) return;
		assert(child != &owner);

		if (addReference)
		{
			// ponytail: a shared model root stays parentless because one raw parent
			// cannot represent multiple instances. Split asset and instance nodes
			// when shared roots need upward traversal.
			assert(child->m_pParent == nullptr);
			child->m_pParent = nullptr;
			child->AddRef();
		}
		else
		{
			child->m_pParent = &owner;
		}

		if (owner.m_pChild)
		{
			child->m_pSibling = owner.m_pChild->m_pSibling;
			owner.m_pChild->m_pSibling = child;
		}
		else
		{
			owner.m_pChild = child;
		}
	}

	template <typename T>
	void ReleaseHierarchy(T*& parent, T*& child, T*& sibling) noexcept
	{
		T* ownedChild = child;
		T* ownedSibling = sibling;
		parent = nullptr;
		child = nullptr;
		sibling = nullptr;
		if (ownedChild) ownedChild->Release();
		if (ownedSibling) ownedSibling->Release();
	}

	template <typename T>
	struct Releaser
	{
		void operator()(T* object) const noexcept
		{
			if (object) object->Release();
		}
	};

	template <typename T>
	using Owner = std::unique_ptr<T, Releaser<T>>;
}
