#ifdef SCALAR_INT_FACET
SCALAR_INT_FACET(I1, 1, llvm::Type::getInt1Ty(ctx))
SCALAR_INT_FACET(I8, 8, llvm::Type::getInt8Ty(ctx))
SCALAR_INT_FACET(I16, 16, llvm::Type::getInt16Ty(ctx))
SCALAR_INT_FACET(I32, 32, llvm::Type::getInt32Ty(ctx))
SCALAR_INT_FACET(I64, 64, llvm::Type::getInt64Ty(ctx))
SCALAR_INT_FACET(I128, 128, llvm::Type::getInt128Ty(ctx))
#endif
#ifdef SCALAR_FP_FACET
SCALAR_FP_FACET(F32, 32, llvm::Type::getFloatTy(ctx))
SCALAR_FP_FACET(F64, 64, llvm::Type::getDoubleTy(ctx))
#endif
#ifdef SPECIAL_FACET
// Special facets for general-purpose registers
SPECIAL_FACET(PTR, 64, llvm::PointerType::get(ctx, 0))
SPECIAL_FACET(I8H, 8, llvm::Type::getInt8Ty(ctx))
#endif
#ifdef PSEUDO_INT_FACET
PSEUDO_INT_FACET(I)
#endif
