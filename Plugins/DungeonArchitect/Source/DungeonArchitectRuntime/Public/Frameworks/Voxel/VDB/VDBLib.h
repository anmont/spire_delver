//$ Copyright 2015-24, Code Respawn Technologies Pvt Ltd - All Rights Reserved $//

#pragma once
#include "CoreMinimal.h"

namespace DAVDB {
	struct FRootKey;
	template<int32 Count>
	class FBitMask {
	public:
		bool operator[](int32 Index) const {
			const uint32 Idx = Index >> Log2;
			const uint32 Off = Index & ContainerSizeMinusOne;
			return ((Mask[Idx] >> Off) & 1UL) != 0;
		}

		void Set(int32 Index, bool InValue) {
			const uint32 Idx = Index >> Log2;
			const uint32 Off = Index & ContainerSizeMinusOne;
			if (InValue) {
				Mask[Idx] |= 1ULL << Off;
			}
			else {
				Mask[Idx] &= ~(1ULL << Off);
			}
		}
		
		typedef uint64 MaskContainer_t;
		static constexpr uint32 ContainerSize = 64;
		static constexpr uint32 ContainerSizeMinusOne = ContainerSize - 1;
		static constexpr uint32 Log2 = 6;
		
		MaskContainer_t Mask[(Count - 1) / ContainerSize + 1] = {};
	};

	template<typename TNode>
	static FORCEINLINE FIntVector CalculateOrigin(const FIntVector& InLocation) {
		return FIntVector {
			InLocation.X & ~((1 << TNode::LOG2X) - 1),
			InLocation.Y & ~((1 << TNode::LOG2Y) - 1),
			InLocation.Z & ~((1 << TNode::LOG2Z) - 1)
		};
	}
	
	template<typename TValue, int TLog2X, int TLog2Y=TLog2X, int TLog2Z=TLog2Y>
	class TLeafNode {
	public:
		using ValueType = TValue;
		
		static constexpr int32 SIZE = 1 << (TLog2X + TLog2Y + TLog2Z);
		static constexpr int32 LOG2X = TLog2X;
		static constexpr int32 LOG2Y = TLog2Y;
		static constexpr int32 LOG2Z = TLog2Z;
		
		struct FLeafData {
			TValue Values[SIZE];
		};
		FLeafData LeafDAT;
		FBitMask<SIZE> ValueMask;
		uint64 Flags = 0;

		bool Get(const FIntVector& InLocation, ValueType& OutValue) {
			const int32 Offset = GetLeafOffset(InLocation);
			check(Offset >= 0 && Offset < SIZE);

			const bool bValueActive = ValueMask[Offset];
			if (bValueActive) {
				OutValue = LeafDAT.Values[Offset];
			}
			return bValueActive;
		}
		
		void Set(const FIntVector& InLocation, const ValueType& InValue) {
			const int32 Offset = GetLeafOffset(InLocation);
			check(Offset >= 0 && Offset < SIZE);

			ValueMask.Set(Offset, true);
			LeafDAT.Values[Offset] = InValue;
		}
		
		static FORCEINLINE FIntVector GetOrigin(const FIntVector& InLocation) {
			return CalculateOrigin<TLeafNode>(InLocation);
		}
		
		static TLeafNode* CreateNew(const FIntVector& InLocation) {
			TLeafNode* NewNode = new TLeafNode();
			const FIntVector Origin = GetOrigin(InLocation);
			constexpr uint32 Bit20Mask = (1 << 20) - 1;
			NewNode->Flags =
				(static_cast<uint64>(Origin.X & Bit20Mask) << 40) +
				(static_cast<uint64>(Origin.Y & Bit20Mask) << 20) +
				static_cast<uint64>(Origin.Z & Bit20Mask);
			
			return NewNode;
		}
		
	private:
		static FORCEINLINE int32 GetLeafOffset(const FIntVector& InLocation) {
			return
				((InLocation.X & (1 << LOG2X) - 1) << (TLog2Y + TLog2Z)) +
				((InLocation.Y & (1 << LOG2Y) - 1) << TLog2Z) +
				(InLocation.Z & (1 << LOG2Z) - 1);
		}
	};


	template<typename TChild, int TLog2X, int TLog2Y=TLog2X, int TLog2Z=TLog2Y>
	class TInternalNode {
	public:
		using ValueType = typename TChild::ValueType;
		using ChildType = TChild;

		static constexpr int32 SIZE = 1 << (TLog2X + TLog2Y + TLog2Z);
		static constexpr int32 LOG2X = TLog2X + TChild::LOG2X;
		static constexpr int32 LOG2Y = TLog2Y + TChild::LOG2Y;
		static constexpr int32 LOG2Z = TLog2Z + TChild::LOG2Z;
		
		union FInternalData {
			TChild* Child = {};		// Child node pointer	
			ValueType Value;		// Tile value
		};
		FInternalData InternalDAT[SIZE] = {};
		FBitMask<SIZE> ValueMask;
		FBitMask<SIZE> ChildMask;
		FIntVector Origin;

		~TInternalNode() {
			for (int i = 0; i < SIZE; i++) {
				if (InternalDAT[i].Child) {
					delete InternalDAT[i].Child;
					InternalDAT[i].Child = nullptr;
				}
			}
		}

		bool Get(const FIntVector& InLocation, ValueType& OutValue) {
			const int32 Offset = GetInternalOffset(InLocation);
			check(Offset >= 0 && Offset < SIZE);
			
			if (ChildMask[Offset]) {
				return InternalDAT[Offset].Child->Get(InLocation, OutValue);
			}
			else {
				const bool bValueActive = ValueMask[Offset];
				if (bValueActive) {
					OutValue = InternalDAT[Offset].Value; 
				}
				return bValueActive;
			}
		}

		void Set(const FIntVector& InLocation, const ValueType& InValue) {
			const int32 Offset = GetInternalOffset(InLocation);
			check(Offset >= 0 && Offset < SIZE);

			if (!ChildMask[Offset]) {
				ChildMask.Set(Offset, true);
				check(!InternalDAT[Offset].Child);
				InternalDAT[Offset].Child = TChild::CreateNew(InLocation);
			}

			InternalDAT[Offset].Child->Set(InLocation, InValue);
		}

		static TInternalNode* CreateNew(const FIntVector& InLocation) {
			TInternalNode* NewNode = new TInternalNode();
			NewNode->Origin = GetOrigin(InLocation);
			return NewNode;
		}
		
		static FORCEINLINE FIntVector GetOrigin(const FIntVector& InLocation) {
			return CalculateOrigin<TInternalNode>(InLocation);
		}
		
		static FORCEINLINE int32 GetInternalOffset(const FIntVector& InLocation) {
			constexpr int32 TLog2YZ = TLog2Y + TLog2Z;
			return
				(((InLocation.X & (1 << LOG2X) - 1) >> TChild::LOG2X) << TLog2YZ) +
				(((InLocation.Y & (1 << LOG2Y) - 1) >> TChild::LOG2Y) << TLog2Z) +
				((InLocation.Z & (1 << LOG2Z) - 1) >> TChild::LOG2Z);
		}

	};

	struct FRootKey {
		int32 X;
		int32 Y;
		int32 Z;
	};

	template<typename TChild>
	class TRootNode {
	public:
		using ValueType = typename TChild::ValueType;
		using ChildType = TChild;
		
		struct FRootData {
			TChild* Node = nullptr;
			bool TileState = false;
			ValueType TileValue = {};
		};
		TMap<FRootKey, FRootData> RootMap;
		ValueType BackgroundValue;

		~TRootNode() {
			Clear();
		}

		static FORCEINLINE FRootKey CreateRootKey(const FIntVector& InLocation) {
			return FRootKey {
				InLocation.X & ~((1 << TChild::LOG2X) - 1),
				InLocation.Y & ~((1 << TChild::LOG2Y) - 1),
				InLocation.Z & ~((1 << TChild::LOG2Z) - 1)
			};
		}

		bool Get(const FIntVector& InLocation, ValueType& OutValue) {
			const FRootKey RootKey = CreateRootKey(InLocation);
			const FRootData* RootData = RootMap.Find(RootKey);
			if (!RootData) {
				OutValue = BackgroundValue;
				return false;
			}
			if (RootData->Node) {
				return RootData->Node->Get(InLocation, OutValue);
			}
			
			OutValue = RootData->TileValue;
			return RootData->TileState;
		}
		
		void Set(const FIntVector& InLocation, const ValueType& InValue) {
			const FRootKey RootKey = CreateRootKey(InLocation);
			FRootData& RootData = RootMap.FindOrAdd(RootKey);
			if (!RootData.Node) {
				// Create a new child node here
				RootData.Node = TChild::CreateNew(InLocation);
			}

			RootData.Node->Set(InLocation, InValue);
		}

		void Clear() {
			for (auto& Entry : RootMap) {
				FRootData& RootData = Entry.Value;
				if (RootData.Node) {
					delete RootData.Node;
				}
			}
			RootMap.Reset();
		}
	};

	template<typename TRootNode>
	class TAccessCache {
	public:
		using ValueType = typename TRootNode::ValueType;
		typedef typename TRootNode::ChildType FNodeLOD2_t;
		typedef typename FNodeLOD2_t::ChildType FNodeLOD1_t;
		typedef typename FNodeLOD1_t::ChildType FNodeLOD0_t;

		TAccessCache(TRootNode* InRootNode) : RootNode(InRootNode) {}
		
		TRootNode* RootNode = nullptr;
		
		FIntVector Node0Key = {};
		FNodeLOD0_t* Node0 = nullptr;
		
		FIntVector Node1Key = {};
		FNodeLOD1_t* Node1 = nullptr;
		
		FIntVector Node2Key = {};
		FNodeLOD2_t* Node2 = nullptr;

		bool Get(const FIntVector& InLocation, ValueType& OutValue) {
			if (Node0 != nullptr && Node0Key == FNodeLOD0_t::GetOrigin(InLocation)) {
				return Get(Node0, InLocation, OutValue);
			}
			
			if (Node1 != nullptr && Node1Key == FNodeLOD1_t::GetOrigin(InLocation)) {
				return Get(Node1, InLocation, OutValue);
			}
			
			if (Node2 != nullptr && Node2Key == FNodeLOD2_t::GetOrigin(InLocation)) {
				return Get(Node2, InLocation, OutValue);
			}

			return Get(RootNode, InLocation, OutValue);
		}
		
		void Set(const FIntVector& InLocation, const ValueType& InValue) {
			if (Node0 != nullptr && Node0Key == FNodeLOD0_t::GetOrigin(InLocation)) {
				Set(Node0, InLocation, InValue);
			}
			else if (Node1 != nullptr && Node1Key == FNodeLOD1_t::GetOrigin(InLocation)) {
				Set(Node1, InLocation, InValue);
			}
			else if (Node2 != nullptr && Node2Key == FNodeLOD2_t::GetOrigin(InLocation)) {
				Set(Node2, InLocation, InValue);
			}
			else {
				Set(RootNode, InLocation, InValue);
			}
		}
		
	private:
		// Getter functions
		FORCEINLINE bool Get(FNodeLOD0_t* InNode, const FIntVector& InLocation, ValueType& OutValue) {
			CacheNode(InNode, &Node0, &Node0Key, InLocation);
			return InNode->Get(InLocation, OutValue);
		}

		FORCEINLINE bool Get(FNodeLOD1_t* InNode, const FIntVector& InLocation, ValueType& OutValue) {
			CacheNode(InNode, &Node1, &Node1Key, InLocation);
			return GetInternal(InNode, InLocation, OutValue);
		}
		
		FORCEINLINE bool Get(FNodeLOD2_t* InNode, const FIntVector& InLocation, ValueType& OutValue) {
			CacheNode(InNode, &Node2, &Node2Key, InLocation);
			return GetInternal(InNode, InLocation, OutValue);
		}

		bool Get(TRootNode* InNode, const FIntVector& InLocation, ValueType& OutValue) {
			const FRootKey RootKey = InNode->CreateRootKey(InLocation);
			const typename TRootNode::FRootData* RootData = InNode->RootMap.Find(RootKey);
			if (!RootData) {
				OutValue = InNode->BackgroundValue;
				return false;
			}
			if (RootData->Node) {
				return Get(RootData->Node, InLocation, OutValue);
			}
			
			OutValue = RootData->TileValue;
			return RootData->TileState;
		}
		
		template<typename TNode>
		FORCEINLINE bool GetInternal(TNode* InNode, const FIntVector& InLocation, ValueType& OutValue) {
			const int32 Offset = TNode::GetInternalOffset(InLocation);
			check(Offset >= 0 && Offset < TNode::SIZE);
			
			if (InNode->ChildMask[Offset]) {
				typename TNode::ChildType* ChildNode = InNode->InternalDAT[Offset].Child;
				return Get(ChildNode, InLocation, OutValue);
			}
			else {
				const bool bValueActive = InNode->ValueMask[Offset];
				if (bValueActive) {
					OutValue = InNode->InternalDAT[Offset].Value; 
				}
				return bValueActive;
			}
		}

		// Setter functions
		FORCEINLINE void Set(FNodeLOD0_t* InNode, const FIntVector& InLocation, const ValueType& InValue) {
			CacheNode(InNode, &Node0, &Node0Key, InLocation);
			InNode->Set(InLocation, InValue);
		}
		
		FORCEINLINE void Set(FNodeLOD1_t* InNode, const FIntVector& InLocation, const ValueType& InValue) {
			CacheNode(InNode, &Node1, &Node1Key, InLocation);
			return SetInternal(InNode, InLocation, InValue);
		}
		
		FORCEINLINE void Set(FNodeLOD2_t* InNode, const FIntVector& InLocation, const ValueType& InValue) {
			CacheNode(InNode, &Node2, &Node2Key, InLocation);
			return SetInternal(InNode, InLocation, InValue);
		}
		
		void Set(TRootNode* InNode, const FIntVector& InLocation, const ValueType& InValue) {
			using ChildType = typename TRootNode::ChildType;
			const FRootKey RootKey = TRootNode::CreateRootKey(InLocation);
			typename TRootNode::FRootData& RootData = InNode->RootMap.FindOrAdd(RootKey);
			if (!RootData.Node) {
				// Create a new child node here
				RootData.Node = ChildType::CreateNew(InLocation);
			}

			Set(RootData.Node, InLocation, InValue);
		}
		
		template<typename TNode>
		FORCEINLINE void SetInternal(TNode* InNode, const FIntVector& InLocation, const ValueType& InValue) {
			using ChildType = typename TNode::ChildType;
			const int32 Offset = TNode::GetInternalOffset(InLocation);
			check(Offset >= 0 && Offset < TNode::SIZE);

			if (!InNode->ChildMask[Offset]) {
				InNode->ChildMask.Set(Offset, true);
				check(!InNode->InternalDAT[Offset].Child);
				InNode->InternalDAT[Offset].Child = ChildType::CreateNew(InLocation);
			}

			Set(InNode->InternalDAT[Offset].Child, InLocation, InValue);
		}
		
		
		// Access Caching functions
		template<typename TNode>
		FORCEINLINE void CacheNode(TNode* InNode, TNode** CacheNodePtr, FIntVector* CacheNodeKeyPtr, const FIntVector& InLocation) {
			if (*CacheNodePtr != InNode) {
				*CacheNodePtr = InNode;
				*CacheNodeKeyPtr = TNode::GetOrigin(InLocation);
			}
		}
	};

	template<typename TRootNode>
	class TVoxelGrid {
	public:
		using ValueType = typename TRootNode::ValueType;
		typedef typename TRootNode::ChildType FNodeLOD2_t;
		typedef typename FNodeLOD2_t::ChildType FNodeLOD1_t;
		typedef typename FNodeLOD1_t::ChildType FNodeLOD0_t;

		TVoxelGrid() : AccessCache(TAccessCache(&RootNode))
		{
		}
		
		bool Get(const FIntVector& InLocation, ValueType& OutValue) {
			return AccessCache.Get(InLocation, OutValue);
		}
				
		void Set(const FIntVector& InLocation, const ValueType& InValue) {
			AccessCache.Set(InLocation, InValue);
		}

		void Clear() {
			RootNode.Clear();
		}
		
	private:
		TRootNode RootNode;
		TAccessCache<TRootNode> AccessCache;
	};

	typedef uint8 VoxelMaterial_t;
	struct FVoxelData {
		VoxelMaterial_t Material = {};
	};
	typedef TLeafNode<FVoxelData, 3> FNodeLOD0;
	typedef TInternalNode<FNodeLOD0, 4> FNodeLOD1;
	typedef TInternalNode<FNodeLOD1, 5> FNodeLOD2;
	typedef TRootNode<FNodeLOD2> FRootNode;
	typedef TVoxelGrid<FRootNode> FVoxelGrid;
	
	FORCEINLINE uint32 GetTypeHash(const DAVDB::FRootKey& InKey) {
		constexpr int32 Log2N = 20;
		return ((1 << Log2N) - 1) & (InKey.X * 73856093 ^ InKey.Y * 19349663 ^ InKey.Z * 83492791);
	}

	FORCEINLINE bool operator==(const DAVDB::FRootKey& A, const DAVDB::FRootKey& B) {
		return A.X == B.X && A.Y == B.Y && A.Z == B.Z;
	}
}

