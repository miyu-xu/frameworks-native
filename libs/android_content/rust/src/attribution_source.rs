// Copyright (C) 2024 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! Rust implementation of `AttributionSource`.

use binder::{
    binder_impl::{BorrowedParcel, UnstructuredParcelable},
    impl_deserialize_for_unstructured_parcelable, impl_serialize_for_unstructured_parcelable,
    StatusCode,
};
use framework_permission_aidl::aidl::android::content::AttributionSourceState::AttributionSourceState;

/// Rust implementation of `AttributionSource`.
pub struct AttributionSource {
    state: AttributionSourceState,
}

impl UnstructuredParcelable for AttributionSource {
    fn write_to_parcel(&self, parcel: &mut BorrowedParcel) -> Result<(), StatusCode> {
        parcel.write(&self.state)?;
        Ok(())
    }

    fn from_parcel(parcel: &BorrowedParcel) -> Result<Self, StatusCode> {
        let state = parcel.read()?;
        Ok(Self { state })
    }
}

impl_deserialize_for_unstructured_parcelable!(AttributionSource);
impl_serialize_for_unstructured_parcelable!(AttributionSource);
