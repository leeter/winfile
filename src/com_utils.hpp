/********************************************************************

com_utils.hpp

Utilities for working with COM

Copyright (c) Microsoft Corporation. All rights reserved.
Licensed under the MIT License.

********************************************************************/
#pragma once

#include <Ole2.h>

namespace com_utils {

	struct stg_medium : public STGMEDIUM {
		~stg_medium()
		{
			InternalRelease();
		}
		stg_medium* operator&() = delete;
		stg_medium* GetAddressOf() noexcept {
			return this;
		}
		stg_medium* ReleaseAndGetAddressOf() noexcept
		{
			InternalRelease();
			return this;
		}
	private:
		void InternalRelease() noexcept {
			if (tymed != TYMED_NULL) {
				ReleaseStgMedium(this);
			}
		}
	};
}
