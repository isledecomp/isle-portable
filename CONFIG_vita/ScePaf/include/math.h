/*
	Vita Development Suite Libraries
*/

#ifndef _VDSUITE_USER_PAF_MATH_MATH_H
#define _VDSUITE_USER_PAF_MATH_MATH_H


#include <paf/math/ngp/misc.h>

#include <psp2/types.h>

namespace paf {
	namespace math {
		class v1 {
		private:
			SceFPlane impl;
		};

		class v2 {
		public:
			bool operator!=(const v2& rh){
				if(this->_x != rh._x){
					return true;
				}
				if(this->_y != rh._y){
					return true;
				}

				return false;
			}

			bool operator==(const v2& rh){
				return !(*this != rh);
			}

			float extract_x() const {
				return _x;
			}

			float extract_y() const {
				return _y;
			}

		private:
			float _x;
			float _y;
		};

		class v3 {
		public:
			bool operator!=(const v3& rh){
				if(this->_impl.x != rh._impl.x){
					return true;
				}
				if(this->_impl.y != rh._impl.y){
					return true;
				}
				if(this->_impl.z != rh._impl.z){
					return true;
				}
/*
				if(this->impl.d != rh.impl.d){
					return true;
				}
*/

				return false;
			}

			bool operator==(const v3& rh){
				return !(*this != rh);
			}

			float extract_x() const
			{
				return _impl.x;
			}

			float extract_y() const
			{
				return _impl.y;
			}

			float extract_z() const
			{
				return _impl.z;
			}

		private:
			SceFVector4 _impl;
		};

		class v4 {
		public:
			v4(){
			}

			v4(float a, float b, float c = 0.0f, float d = 0.0f){
				impl.a = a;
				impl.b = b;
				impl.c = c;
				impl.d = d;
			}

			static v4 _0000(){
				return v4(0.0f, 0.0f, 0.0f, 0.0f);
			}

			bool operator!=(const v4& rh){
				if(this->impl.a != rh.impl.a){
					return true;
				}
				if(this->impl.b != rh.impl.b){
					return true;
				}
				if(this->impl.c != rh.impl.c){
					return true;
				}
				if(this->impl.d != rh.impl.d){
					return true;
				}

				return false;
			}

			bool operator==(const v4& rh){
				return !(*this != rh);
			}

		private:
			SceFPlane impl;
		};
		class matrix {
		private:
			SceFMatrix4 impl;
		};
		class quaternion {
		private:
			SceFQuaternion impl;
		};
	}
}

/*
#include <paf/math/ngp/v1.h>
#include <paf/math/ngp/v1_impl.h>
#include <paf/math/ngp/v2.h>
#include <paf/math/ngp/v2_impl.h>
#include <paf/math/ngp/v3.h>
#include <paf/math/ngp/v3_impl.h>
#include <paf/math/ngp/v4.h>
#include <paf/math/ngp/v4_impl.h>
#include <paf/math/ngp/matrix.h>
#include <paf/math/ngp/matrix3x3.h>
#include <paf/math/ngp/quaternion.h>
#include <paf/math/ngp/spherical_coords.h>
#include <paf/math/rect.h>
*/


#endif /* _VDSUITE_USER_PAF_MATH_MATH_H */ 