// Field is a DiscreteSpace with data assigned to the cells, and operations on the data.
// The data is some fixed number of floating point values per cell.
// The storage pattern is variable, and can be specified by designating a packed axis.
// The topology (dimensions, ghost cells) is inherited from the DiscreteSpace passed to the constructor.
// It is possible to mix Fields with varying ghost cell layers, but it is MUCH SAFER
// to keep the ghost cell layers the same for all Field instances in a calculation.

// Note the Field does not inherit from MetricSpace.  The intention is to have only a single
// instance of MetricSpace active at any time (mainly to avoid replicating the metric data)

enum boundarySpec
{
	normalFluxFixed,
	dirichletWall,
	neumannWall,
	dirichletCell,
	natural,
	periodic,
	none
};

enum axisSpec { tAxis , xAxis , yAxis , zAxis };
enum sideSpec { lowSide , highSide };

inline tw::Int naxis(const axisSpec& axis)
{
	switch (axis)
	{
		case tAxis:
			return 0;
		case xAxis:
			return 1;
		case yAxis:
			return 2;
		case zAxis:
			return 3;
		default:
			return 0;
	}
}

inline axisSpec enumaxis(tw::Int ax)
{
	switch (ax)
	{
		case 0:
			return tAxis;
		case 1:
			return xAxis;
		case 2:
			return yAxis;
		case 3:
			return zAxis;
		default:
			return tAxis;
	}
}

template <class T>
struct Matrix
{
	std::valarray<T> array;
	tw::Int rows,cols;
	Matrix()
	{
		rows = 3;
		cols = 3;
		array.resize(9);
	}
	Matrix(tw::Int r,tw::Int c)
	{
		rows = r;
		cols = c;
		array.resize(r*c);
	}
	void resize(tw::Int r,tw::Int c)
	{
		rows = r;
		cols = c;
		array.resize(r*c);
	}
	T& operator () (const tw::Int& r,const tw::Int& c)
	{
		return array[r*cols + c];
	}
	T operator () (const tw::Int& r,const tw::Int& c) const
	{
		return array[r*cols + c];
	}
	Matrix<T>& operator = (Matrix<T>& src)
	{
		rows = src.rows;
		cols = src.cols;
		array = src.array;
		return *this;
	}
};

struct BoundaryCondition
{
	// General BC consists of a folding operation and a forcing operation.
	// The folding operation is used to clean up after source deposition operations.
	// Typically folding operates on volume integrated states.
	// Typically forcing operates on density states.
	// Folding: strip_i = sum_j(fold_ij * strip_j)
	// Forcing: strip_i = sum_j(force_ij * strip_j) + coeff_i * BC
	// Setting a particular boundary condition consists of defining the matrices.

	// WARNING: this object specialized for fields with 2 ghost cell layers

	tw::Float fold[4][4];
	tw::Float force[4][4];
	tw::Float coeff[4];
	tw::Int sgn;

	BoundaryCondition();
	void Reset();
	void Set(boundarySpec theBoundaryCondition,sideSpec whichSide);
	void FoldingOperation(tw::Float* strip,tw::Int stride)
	{
		// strip should point at the outermost ghost cell involved
		tw::Float temp[4];
		for (tw::Int i=0;i<4;i++)
		{
			temp[i] = strip[i*stride*sgn];
			strip[i*stride*sgn] = 0.0;
		}
		for (tw::Int i=0;i<4;i++)
			for (tw::Int j=0;j<4;j++)
				strip[i*stride*sgn] += fold[i][j]*temp[j];
	}
	void ForcingOperation(tw::Float* strip,tw::Int stride,tw::Float BC)
	{
		// strip should point at the outermost ghost cell involved
		// BC is the value at the wall (dirichletWall) or in ghost cell (dirichletCell), or the difference of adjacent cells (neumannWall)
		tw::Float temp[4];
		for (tw::Int i=0;i<4;i++)
		{
			temp[i] = strip[i*stride*sgn];
			strip[i*stride*sgn] = coeff[i]*BC;
		}
		for (tw::Int i=0;i<4;i++)
			for (tw::Int j=0;j<4;j++)
				strip[i*stride*sgn] += force[i][j]*temp[j];
	}
};

struct Element
{
	// index vector components from zero
	tw::Int low,high;
	Element();
	Element(tw::Int l,tw::Int h);
	Element(tw::Int i);
	tw::Int Components() const;
	friend Element Union(const Element& e1,const Element& e2);
	Element operator () (const tw::Int& i1,const tw::Int& i2) const
	{
		return Element(low+i1,low+i2);
	}
	Element operator () (const tw::Int& i) const
	{
		return Element(low+i,low+i);
	}
};

template <class T>
struct Slice
{
	Element e;
	tw::Int lb[4],ub[4]; // bounds of outer surface
	private:
	tw::Int decodingStride[4];
	tw::Int encodingStride[4];
	std::valarray<T> data;
	public:
	Slice() {;}
	Slice(const Element& e,tw::Int xl,tw::Int xh,tw::Int yl,tw::Int yh,tw::Int zl,tw::Int zh);
	Slice(const Element& e,tw::Int low[4],tw::Int high[4],tw::Int ignorable[4]);
	void Resize(const Element& e,tw::Int low[4],tw::Int high[4],tw::Int ignorable[4]);
	T* Buffer()
	{
		return &data[0];
	}
	tw::Int BufferSize()
	{
		return sizeof(T)*e.Components()*(ub[1]-lb[1]+1)*(ub[2]-lb[2]+1)*(ub[3]-lb[3]+1);
	}
	void Translate(const axisSpec& axis,tw::Int displ);
	void Translate(tw::Int x,tw::Int y,tw::Int z);
	T& operator () (const tw::Int& x,const tw::Int& y,const tw::Int& z,const tw::Int& c)
	{
		return data[(c-e.low) + (x-lb[1])*encodingStride[1] + (y-lb[2])*encodingStride[2] + (z-lb[3])*encodingStride[3]];
	}
	T operator () (const tw::Int& x,const tw::Int& y,const tw::Int& z,const tw::Int& c) const
	{
		return data[(c-e.low) + (x-lb[1])*encodingStride[1] + (y-lb[2])*encodingStride[2] + (z-lb[3])*encodingStride[3]];
	}
	T operator () (const StripIterator& strip,const tw::Int& i,const tw::Int& c) const
	{
		tw::Int x,y,z;
		strip.Decode(i,&x,&y,&z);
		return (*this)(x,y,z,c);
	}
	Slice<T>& operator = (T a)
	{
		data = a;
		return *this;
	}
};

struct Field:DiscreteSpace
{
	protected:

	tw::Int totalCells;
	tw::Int boundaryCells;
	tw::Int ghostCells;
	Matrix<BoundaryCondition> bc0,bc1;
	tw::Int packedAxis,bufferState;
	// Indexing in the following has 0=components, (1,2,3)=spatial axes
	// This is an encoding stride only, some may be zero
	tw::Int stride[4];

	void BoundaryDataToField();
	void FieldToBoundaryData();
	void FieldToGhostData();

	public:

	std::valarray<tw::Float> array;
	std::valarray<tw::Float> boundaryData;
	std::valarray<tw::Int> boundaryDataIndexMap;
	std::valarray<tw::Float> ghostData;
	std::valarray<tw::Int> ghostDataIndexMap;
	Task *task;
	#ifdef USE_OPENCL
	cl_mem computeBuffer;
	cl_mem boundaryBuffer;
	cl_mem boundaryMapBuffer;
	cl_mem ghostBuffer;
	cl_mem ghostMapBuffer;
	#endif

	// SETUP

	Field();
	~Field();
	void Initialize(tw::Int components,const DiscreteSpace& ds,Task *task,const axisSpec& axis);
	void Initialize(tw::Int components,const DiscreteSpace& ds,Task *task) { Initialize(components,ds,task,zAxis); }
	void SetBoundaryConditions(const Element& e,const axisSpec& axis,boundarySpec low,boundarySpec high);
	friend void CopyBoundaryConditions(Field& dst,const Element& dstElement,Field& src,const Element& srcElement);

	// ACCESSORS

	// Conventional accessors using fortran style array indexing
	tw::Float& operator () (const tw::Int& x,const tw::Int& y,const tw::Int& z,const tw::Int& c)
	{
		return array[c*stride[0] + (x-lb[1])*stride[1] + (y-lb[2])*stride[2] + (z-lb[3])*stride[3]];
	}
	tw::Float operator () (const tw::Int& x,const tw::Int& y,const tw::Int& z,const tw::Int& c) const
	{
		return array[c*stride[0] + (x-lb[1])*stride[1] + (y-lb[2])*stride[2] + (z-lb[3])*stride[3]];
	}
	// Accessors for stepping through cells without concern for direction
	tw::Float& operator () (const CellIterator& cell,const tw::Int& c)
	{
		return array[cell.Index(c,stride)];
	}
	tw::Float operator () (const CellIterator& cell,const tw::Int& c) const
	{
		return array[cell.Index(c,stride)];
	}
	// Accessors for iterating across strips
	// Given a strip, returns component c in cell s as measured along the strip
	tw::Float& operator () (const StripIterator& strip,const tw::Int& s,const tw::Int& c)
	{
		return array[strip.Index(s,c,stride)];
	}
	tw::Float operator () (const StripIterator& strip,const tw::Int& s,const tw::Int& c) const
	{
		return array[strip.Index(s,c,stride)];
	}
	// Accessors for promoting compiler vectorization
	tw::Float& operator () (const VectorizingIterator<1>& v,const tw::Int& s,const tw::Int& c)
	{
		return array[v.Index1(s,c,stride)];
	}
	tw::Float operator () (const VectorizingIterator<1>& v,const tw::Int& s,const tw::Int& c) const
	{
		return array[v.Index1(s,c,stride)];
	}
	tw::Float& operator () (const VectorizingIterator<3>& v,const tw::Int& s,const tw::Int& c)
	{
		return array[v.Index3(s,c,stride)];
	}
	tw::Float operator () (const VectorizingIterator<3>& v,const tw::Int& s,const tw::Int& c) const
	{
		return array[v.Index3(s,c,stride)];
	}
	// Centered Differencing
	tw::Float operator () (const CellIterator& cell,const tw::Int& c,const tw::Int& ax) const
	{
		const tw::Int idx = cell.Index(c,stride);
		return 0.5*freq[ax-1]*( array[idx + stride[ax]] - array[idx - stride[ax]] );
	}
	tw::Float operator () (const VectorizingIterator<1>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		const tw::Int idx = v.Index1(s,c,stride);
		return 0.5*freq[ax-1]*( array[idx + stride[ax]] - array[idx - stride[ax]] );
	}
	tw::Float operator () (const VectorizingIterator<3>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		const tw::Int idx = v.Index3(s,c,stride);
		return 0.5*freq[ax-1]*( array[idx + stride[ax]] - array[idx - stride[ax]] );
	}
	tw::Float d2(const StripIterator& strip,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		const tw::Int idx = strip.Index(s,c,stride);
		return freq[ax-1]*freq[ax-1]*( array[idx - stride[ax]] - 2.0*array[idx] + array[idx + stride[ax]] );
	}
	tw::Float d2(const VectorizingIterator<1>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		const tw::Int idx = v.Index1(s,c,stride);
		return freq[ax-1]*freq[ax-1]*( array[idx - stride[ax]] - 2.0*array[idx] + array[idx + stride[ax]] );
	}
	tw::Float d2(const VectorizingIterator<3>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		const tw::Int idx = v.Index3(s,c,stride);
		return freq[ax-1]*freq[ax-1]*( array[idx - stride[ax]] - 2.0*array[idx] + array[idx + stride[ax]] );
	}
	// Unit directional offset
	tw::Float fwd(const CellIterator& cell,const tw::Int& c,const tw::Int& ax) const
	{
		return array[cell.Index(c,stride) + stride[ax]];
	}
	tw::Float bak(const CellIterator& cell,const tw::Int& c,const tw::Int& ax) const
	{
		return array[cell.Index(c,stride) - stride[ax]];
	}
	// Forward and Backward Differences and Sums
	tw::Float dfwd(const StripIterator& strip,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		tw::Int idx = strip.Index(s,c,stride);
		return freq[ax-1]*( array[idx + stride[ax]] - array[idx] );
	}
	tw::Float dfwd(const VectorizingIterator<3>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		tw::Int idx = v.Index3(s,c,stride);
		return freq[ax-1]*( array[idx + stride[ax]] - array[idx] );
	}
	tw::Float dbak(const VectorizingIterator<3>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		tw::Int idx = v.Index3(s,c,stride);
		return freq[ax-1]*( array[idx] - array[idx - stride[ax]] );
	}
	tw::Float sfwd(const VectorizingIterator<1>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		tw::Int idx = v.Index1(s,c,stride);
		return 0.5*( array[idx + stride[ax]] + array[idx] );
	}
	tw::Float sbak(const VectorizingIterator<1>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		tw::Int idx = v.Index1(s,c,stride);
		return 0.5*( array[idx] + array[idx - stride[ax]] );
	}
	tw::Float sfwd(const VectorizingIterator<3>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		tw::Int idx = v.Index3(s,c,stride);
		return 0.5*( array[idx + stride[ax]] + array[idx] );
	}
	tw::Float sbak(const VectorizingIterator<3>& v,const tw::Int& s,const tw::Int& c,const tw::Int& ax) const
	{
		tw::Int idx = v.Index3(s,c,stride);
		return 0.5*( array[idx] + array[idx - stride[ax]] );
	}
	friend Element All(const Field& A) { return Element(0,A.num[0]-1); }
	tw::Int Components() const { return num[0]; }
	tw::Int TotalCells() const { return totalCells; }
	size_t TotalBytes() const { return array.size()*sizeof(tw::Float); }
	size_t BoundaryBytes() const { return boundaryData.size()*sizeof(tw::Float); }
	size_t GhostBytes() const { return ghostData.size()*sizeof(tw::Float); }
	size_t BoundaryMapBytes() const { return boundaryDataIndexMap.size()*sizeof(tw::Int); }
	size_t GhostMapBytes() const { return ghostDataIndexMap.size()*sizeof(tw::Int); }
	tw::Int Stride(tw::Int ax) const { return stride[ax]; }
	tw::Int *Stride() { return stride; }

	// GPU Support

	#ifdef USE_OPENCL
	void InitializeComputeBuffer();
	void SendToComputeBuffer();
	void ReceiveFromComputeBuffer();
	void SendBoundaryCellsToComputeBuffer();
	void ReceiveBoundaryCellsFromComputeBuffer();
	tw::Float CellValueInComputeBuffer(tw::Int i,tw::Int j,tw::Int k,tw::Int c); // an expensive call for one data point (useful for last step of reduction)
	void UpdateGhostCellsInComputeBuffer(const Element& e);
	void SendGhostCellsToComputeBuffer();
	void ZeroGhostCellsInComputeBuffer();
	friend void SwapComputeBuffers(Field& f1,Field& f2);
	friend void CopyComputeBuffer(Field& dst,Field& src);
	friend void CopyComplexComputeBufferMod2(Field& dst,Field& src);
	void FillComputeBufferVec4(const Element& e,tw::vec4& A);
	tw::Float DestructiveSumComputeBuffer();
	tw::Float DestructiveNorm1ComputeBuffer();
	void WeightComputeBufferByVolume(MetricSpace& ms,tw::Float inv);
	void DestructiveComplexMod2ComputeBuffer();
	void MADDComputeBuffer(tw::Float m,tw::Float a);
	#endif

	// Transformation

	void MultiplyCellVolume(const MetricSpace& ds);
	void DivideCellVolume(const MetricSpace& ds);
	friend void Swap(Field& f1,Field& f2);
	void Swap(const Element& e1,const Element& e2);
	friend void CopyFieldData(Field& dst,const Element& e_dst,Field& src,const Element& e_src);
	friend void AddFieldData(Field& dst,const Element& e_dst,Field& src,const Element& e_src);
	friend void AddMulFieldData(Field& dst,const Element& e_dst,Field& src,const Element& e_src,tw::Float mul);
	void SmoothingPass(const Element& e,const MetricSpace& ds,const tw::Float& X0,const tw::Float& X1,const tw::Float& X2);
	void Smooth(const Element& e,const MetricSpace& ds,tw::Int smoothPasses,tw::Int compPasses);
	void Shift(const Element& e,const StripIterator& s,tw::Int cells,const tw::Float* incoming);
	void Shift(const Element& e,const StripIterator& s,tw::Int cells,const tw::Float& incoming);

	// Subsets and Slices

	void GetStrip(std::valarray<tw::Float>& cpy,const StripIterator& s,const tw::Int& c)
	{
		for (tw::Int i=0;i<s.N1();i++) // beware assumption of 2 layers
			cpy[i] = (*this)(s,i,c);
	}
	void SetStrip(std::valarray<tw::Float>& cpy,const StripIterator& s,const tw::Int& c)
	{
		for (tw::Int i=0;i<s.N1();i++) // beware assumption of 2 layers
			(*this)(s,i,c) = cpy[i];
	}
	tw::vec3 Vec3(const tw::Int& i,const tw::Int& j,const tw::Int& k,const tw::Int& c) const
	{
		return tw::vec3((*this)(i,j,k,c),(*this)(i,j,k,c+1),(*this)(i,j,k,c+2));
	}
	tw::vec3 Vec3(const CellIterator& cell,const tw::Int& c) const
	{
		return tw::vec3((*this)(cell,c),(*this)(cell,c+1),(*this)(cell,c+2));
	}
	tw::vec3 Vec3(const StripIterator& strip,const tw::Int& s,const tw::Int& c) const
	{
		return tw::vec3((*this)(strip,s,c),(*this)(strip,s,c+1),(*this)(strip,s,c+2));
	}
	template <class T>
	void LoadDataIntoImage(Slice<T>* volume);
	template <class T>
	void SaveDataFromImage(Slice<T>* volume);
	template <class T>
	void ZeroDataInField(Slice<T>* volume);
	template <class T>
	void AddDataFromImage(Slice<T>* volume);
	template <class T>
	void AddDataFromImageAtomic(Slice<T>* volume);

	// Boundaries, messages, smoothing

	void ApplyFoldingCondition(const Element& e);
	void ApplyBoundaryCondition(const Element& e);
	void AdjustTridiagonalForBoundaries(const Element& e,const axisSpec& axis,const sideSpec& side,tw::Float *T1,tw::Float *T2,tw::Float *T3,tw::Float *source,tw::Float *val);
	void ZeroGhostCells(const Element& e);

	void StripCopyProtocol(tw::Int axis,tw::Int shift,Slice<tw::Float> *planeIn,Slice<tw::Float> *planeOut,bool add);
	void DownwardCopy(const axisSpec& axis,const Element& e,tw::Int cells);
	void UpwardCopy(const axisSpec& axis,const Element& e,tw::Int cells);
	void DownwardDeposit(const axisSpec& axis,const Element& e,tw::Int cells);
	void UpwardDeposit(const axisSpec& axis,const Element& e,tw::Int cells);

	void CopyFromNeighbors(const Element& e);
	void DepositFromNeighbors(const Element& e);

	Slice<tw::Float>* FormTransposeBlock(const Element& e,const axisSpec& axis1,const axisSpec& axis2,tw::Int start1,tw::Int end1,tw::Int start2,tw::Int end2);
	void Transpose(const Element& e,const axisSpec& axis1,const axisSpec& axis2,Field *target,tw::Int inversion);


	// All-element convenience functions

	#ifdef USE_OPENCL
	void UpdateGhostCellsInComputeBuffer()
	{
		UpdateGhostCellsInComputeBuffer(All(*this));
	}
	#endif
	void SetBoundaryConditions(const axisSpec& axis,boundarySpec low,boundarySpec high)
	{
		SetBoundaryConditions(All(*this),axis,low,high);
	}
	void DownwardCopy(const axisSpec& axis,tw::Int cells)
	{
		DownwardCopy(axis,All(*this),cells);
	}
	void UpwardCopy(const axisSpec& axis,tw::Int cells)
	{
		UpwardCopy(axis,All(*this),cells);
	}
	void DownwardDeposit(const axisSpec& axis,tw::Int cells)
	{
		DownwardDeposit(axis,All(*this),cells);
	}
	void UpwardDeposit(const axisSpec& axis,tw::Int cells)
	{
		UpwardDeposit(axis,All(*this),cells);
	}
	void CopyFromNeighbors()
	{
		CopyFromNeighbors(All(*this));
	}
	void DepositFromNeighbors()
	{
		DepositFromNeighbors(All(*this));
	}
	void Transpose(const axisSpec& axis1,const axisSpec& axis2,Field *target,tw::Int inversion)
	{
		Transpose(All(*this),axis1,axis2,target,inversion);
	}
	void ApplyFoldingCondition()
	{
		ApplyFoldingCondition(All(*this));
	}
	void ApplyBoundaryCondition()
	{
		ApplyBoundaryCondition(All(*this));
	}
	void AdjustTridiagonalForBoundaries(const axisSpec& axis,const sideSpec& side,tw::Float *T1,tw::Float *T2,tw::Float *T3,tw::Float *source,tw::Float *val)
	{
		AdjustTridiagonalForBoundaries(All(*this),axis,side,T1,T2,T3,source,val);
	}
	void ZeroGhostCells()
	{
		ZeroGhostCells(All(*this));
	}
	void Smooth(const MetricSpace& ds,tw::Int smoothPasses,tw::Int compPasses)
	{
		Smooth(All(*this),ds,smoothPasses,compPasses);
	}
	void Shift(const StripIterator& s,tw::Int cells,const tw::Float* incoming)
	{
		Shift(All(*this),s,cells,incoming);
	}
	void Shift(const StripIterator& s,tw::Int cells,const tw::Float& incoming)
	{
		Shift(All(*this),s,cells,incoming);
	}

	// File operations

	void ReadData(std::ifstream& inFile);
	void WriteData(std::ofstream& outFile);

	// Assignment

	Field& operator = (Field& A)
	{
		array = A.array;
		return *this;
	}
	Field& operator += (Field& A)
	{
		array += A.array;
		return *this;
	}
	Field& operator -= (Field& A)
	{
		array -= A.array;
		return *this;
	}
	Field& operator *= (Field& A)
	{
		array *= A.array;
		return *this;
	}
	Field& operator /= (Field& A)
	{
		array /= A.array;
		return *this;
	}
	Field& operator = (tw::Float a)
	{
		array = a;
		return *this;
	}
	Field& operator += (tw::Float a)
	{
		array += a;
		return *this;
	}
	Field& operator -= (tw::Float a)
	{
		array -= a;
		return *this;
	}
	Field& operator *= (tw::Float a)
	{
		array *= a;
		return *this;
	}
	Field& operator /= (tw::Float a)
	{
		array /= a;
		return *this;
	}

	// Grid Interpolation

	void Interpolate(std::valarray<tw::Float>& val,const Element& e,const weights_3D& weights);
	void Interpolate(std::valarray<tw::Float>& val,const weights_3D& weights)
	{
		Interpolate(val,All(*this),weights);
	}
	void InterpolateOnto(std::valarray<tw::Float>& val,const Element& e,const weights_3D& weights);
	void InterpolateOnto(std::valarray<tw::Float>& val,const weights_3D& weights)
	{
		InterpolateOnto(val,All(*this),weights);
	}

	template<tw::Int T,tw::Int X,tw::Int Y,tw::Int Z>
	friend void conserved_current_to_dens(Field& current,const MetricSpace& m);
	template <tw::Int C,tw::Int X,tw::Int Y,tw::Int Z>
	friend void assign_grad(const Field& sf,Field& vf,const MetricSpace& m,const tw::Float& scaleFactor);
	template <tw::Int C,tw::Int X,tw::Int Y,tw::Int Z>
	friend void add_grad(const Field& sf,Field& vf,const MetricSpace& m,const tw::Float& scaleFactor);
	template <tw::Int U,tw::Int V,tw::Int W,tw::Int X,tw::Int Y,tw::Int Z>
	friend void add_curlB(const Field& src,Field& dst,const MetricSpace& m,const tw::Float& scaleFactor);
	template <tw::Int U,tw::Int V,tw::Int W,tw::Int X,tw::Int Y,tw::Int Z>
	friend void add_curlE(const Field& src,Field& dst,const MetricSpace& m,const tw::Float& scaleFactor);
	template <tw::Int X,tw::Int Y,tw::Int Z>
	friend tw::Float divE(const Field& vf,tw::Int i,tw::Int j,tw::Int k,const MetricSpace& m);
	template <tw::Int X,tw::Int Y,tw::Int Z>
	friend tw::Float div(const Field& vf,tw::Int i,tw::Int j,tw::Int k,const MetricSpace& m);
	template <tw::Int C,tw::Int X,tw::Int Y,tw::Int Z>
	friend tw::Float div(const Field& coeff,const Field& vf,tw::Int i,tw::Int j,tw::Int k,const MetricSpace& m);
	template <tw::Int C,tw::Int S>
	friend tw::Float div(const Field& coeff,const Field& sf,tw::Int i,tw::Int j,tw::Int k,const MetricSpace& m);
};

template<tw::Int T,tw::Int X,tw::Int Y,tw::Int Z>
inline void conserved_current_to_dens(Field& current,const MetricSpace& m)
{
	#pragma omp parallel
	{
		for (CellIterator cell(m,true);cell<cell.end();++cell)
		{
			current(cell,T) /= m.dS(cell,0);
			current(cell,X) /= m.dS(cell,1) + tw::small_pos;
			current(cell,Y) /= m.dS(cell,2) + tw::small_pos;
			current(cell,Z) /= m.dS(cell,3) + tw::small_pos;
		}
	}
}

template <tw::Int C,tw::Int X,tw::Int Y,tw::Int Z>
inline void assign_grad(const Field& sf,Field& vf,const MetricSpace& m,const tw::Float& scaleFactor)
{
	// assign the gradient of a scalar field (sf) to a vector field (vf)
	const tw::Int xN1=m.N1(1),yN1=m.N1(2),zN1=m.N1(3);
	#pragma omp parallel for collapse(3) schedule(static)
	for (tw::Int i=1;i<=xN1;i++)
		for (tw::Int j=1;j<=yN1;j++)
			for (tw::Int k=1;k<=zN1;k++)
			{
				vf(i,j,k,X) = scaleFactor * (sf(i,j,k,C) - sf(i-1,j,k,C)) / m.dl(i,j,k,1);
				vf(i,j,k,Y) = scaleFactor * (sf(i,j,k,C) - sf(i,j-1,k,C)) / m.dl(i,j,k,2);
				vf(i,j,k,Z) = scaleFactor * (sf(i,j,k,C) - sf(i,j,k-1,C)) / m.dl(i,j,k,3);
			}
}

template <tw::Int C,tw::Int X,tw::Int Y,tw::Int Z>
inline void add_grad(const Field& sf,Field& vf,const MetricSpace& m,const tw::Float& scaleFactor)
{
	// add the gradient of a scalar field (sf) to a vector field (vf)
	const tw::Int xN1=m.N1(1),yN1=m.N1(2),zN1=m.N1(3);
	#pragma omp parallel for collapse(3) schedule(static)
	for (tw::Int i=1;i<=xN1;i++)
		for (tw::Int j=1;j<=yN1;j++)
			for (tw::Int k=1;k<=zN1;k++)
			{
				vf(i,j,k,X) += scaleFactor * (sf(i,j,k,C) - sf(i-1,j,k,C)) / m.dl(i,j,k,1);
				vf(i,j,k,Y) += scaleFactor * (sf(i,j,k,C) - sf(i,j-1,k,C)) / m.dl(i,j,k,2);
				vf(i,j,k,Z) += scaleFactor * (sf(i,j,k,C) - sf(i,j,k-1,C)) / m.dl(i,j,k,3);
			}
}

template <tw::Int U,tw::Int V,tw::Int W,tw::Int X,tw::Int Y,tw::Int Z>
inline void add_curlB(const Field& src,Field& dst,const MetricSpace& m,const tw::Float& scaleFactor)
{
	// curl of B-field on generalized Yee mesh
	VectorizingIterator<3> v(src,true),vi(src,true),vj(src,true);
	const tw::Int xDim=m.Dim(1),yDim=m.Dim(2),zDim=m.Dim(3),xN1=m.N1(1),yN1=m.N1(2),zN1=m.N1(3);
	#pragma omp parallel for firstprivate(v,vi,vj) collapse(2) schedule(static)
	for (tw::Int i=1;i<=xN1;i++)
		for (tw::Int j=1;j<=yDim;j++)
		{
			v.SetStrip(i,j,0);
			vj.SetStrip(i,j+1,0);
			#pragma omp simd
			for (tw::Int k=1;k<=zDim;k++)
			{
				dst(v,k,X) += scaleFactor * (src(v,k,V)*m.dlh(v,k,2) - src(v,k+1,V)*m.dlh(v,k+1,2)) / m.dS(v,k,1);
				dst(v,k,X) += scaleFactor * (src(vj,k,W)*m.dlh(vj,k,3) - src(v,k,W)*m.dlh(v,k,3)) / m.dS(v,k,1);
			}
		}
	#pragma omp parallel for firstprivate(v,vi,vj) collapse(2) schedule(static)
	for (tw::Int i=1;i<=xDim;i++)
		for (tw::Int j=1;j<=yN1;j++)
		{
			v.SetStrip(i,j,0);
			vi.SetStrip(i+1,j,0);
			#pragma omp simd
			for (tw::Int k=1;k<=zDim;k++)
			{
				dst(v,k,Y) -= scaleFactor * (src(v,k,U)*m.dlh(v,k,1) - src(v,k+1,U)*m.dlh(v,k+1,1)) / m.dS(v,k,2);
				dst(v,k,Y) -= scaleFactor * (src(vi,k,W)*m.dlh(vi,k,3) - src(v,k,W)*m.dlh(v,k,3)) / m.dS(v,k,2);
			}
		}
	#pragma omp parallel for firstprivate(v,vi,vj) collapse(2) schedule(static)
	for (tw::Int i=1;i<=xDim;i++)
		for (tw::Int j=1;j<=yDim;j++)
		{
			v.SetStrip(i,j,0);
			vi.SetStrip(i+1,j,0);
			vj.SetStrip(i,j+1,0);
			#pragma omp simd
			for (tw::Int k=1;k<=zN1;k++)
			{
				dst(v,k,Z) += scaleFactor * (src(v,k,U)*m.dlh(v,k,1) - src(vj,k,U)*m.dlh(vj,k,1)) / m.dS(v,k,3);
				dst(v,k,Z) += scaleFactor * (src(vi,k,V)*m.dlh(vi,k,2) - src(v,k,V)*m.dlh(v,k,2)) / m.dS(v,k,3);
			}
		}
}

template <tw::Int U,tw::Int V,tw::Int W,tw::Int X,tw::Int Y,tw::Int Z>
inline void add_curlE(const Field& src,Field& dst,const MetricSpace& m,const tw::Float& scaleFactor)
{
	// curl of E-field on generalized Yee mesh
	VectorizingIterator<3> v(src,true),vi(src,true),vj(src,true);
	const tw::Int xN1=m.N1(1),yN1=m.N1(2),zN1=m.N1(3);
	#pragma omp parallel for firstprivate(v,vi,vj) collapse(2) schedule(static)
	for (tw::Int i=1;i<=xN1;i++)
		for (tw::Int j=1;j<=yN1;j++)
		{
			v.SetStrip(i,j,0);
			vi.SetStrip(i-1,j,0);
			vj.SetStrip(i,j-1,0);
			#pragma omp simd
			for (tw::Int k=1;k<=zN1;k++)
			{
				dst(v,k,X) += scaleFactor * (src(v,k-1,V)*m.dl(v,k-1,2) - src(v,k,V)*m.dl(v,k,2)) / m.dSh(v,k,1);
				dst(v,k,X) += scaleFactor * (src(v,k,W)*m.dl(v,k,3) - src(vj,k,W)*m.dl(vj,k,3)) / m.dSh(v,k,1);
				dst(v,k,Y) -= scaleFactor * (src(v,k-1,U)*m.dl(v,k-1,1) - src(v,k,U)*m.dl(v,k,1)) / m.dSh(v,k,2);
				dst(v,k,Y) -= scaleFactor * (src(v,k,W)*m.dl(v,k,3) - src(vi,k,W)*m.dl(vi,k,3)) / m.dSh(v,k,2);
				dst(v,k,Z) += scaleFactor * (src(vj,k,U)*m.dl(vj,k,1) - src(v,k,U)*m.dl(v,k,1)) / m.dSh(v,k,3);
				dst(v,k,Z) += scaleFactor * (src(v,k,V)*m.dl(v,k,2) - src(vi,k,V)*m.dl(vi,k,2)) / m.dSh(v,k,3);
			}
		}
}

template <tw::Int X,tw::Int Y,tw::Int Z>
inline tw::Float divE(const Field& vf,tw::Int i,tw::Int j,tw::Int k,const MetricSpace& m)
{
	// divergence of E-field on generalized Yee mesh
	tw::Float ans,vol;
	vol = m.dS(i,j,k,0);
	ans =  vf(i+1,j,k,X)*m.dS(i+1,j,k,1);
	ans -= vf(i,j,k,X)*m.dS(i,j,k,1);
	ans += vf(i,j+1,k,Y)*m.dS(i,j+1,k,2);
	ans -= vf(i,j,k,Y)*m.dS(i,j,k,2);
	ans += vf(i,j,k+1,Z)*m.dS(i,j,k+1,3);
	ans -= vf(i,j,k,Z)*m.dS(i,j,k,3);
	return ans/vol;
}

template <tw::Int X,tw::Int Y,tw::Int Z>
inline tw::Float div(const Field& vf,tw::Int i,tw::Int j,tw::Int k,const MetricSpace& m)
{
	// divergence of a vector field
	tw::Float ans,vol;
	vol = m.dS(i,j,k,0);
	ans =  (vf(i+1,j,k,X)+vf(i,j,k,X))*m.dS(i+1,j,k,1);
	ans -= (vf(i-1,j,k,X)+vf(i,j,k,X))*m.dS(i,j,k,1);
	ans += (vf(i,j+1,k,Y)+vf(i,j,k,Y))*m.dS(i,j+1,k,2);
	ans -= (vf(i,j-1,k,Y)+vf(i,j,k,Y))*m.dS(i,j,k,2);
	ans += (vf(i,j,k+1,Z)+vf(i,j,k,Z))*m.dS(i,j,k+1,3);
	ans -= (vf(i,j,k-1,Z)+vf(i,j,k,Z))*m.dS(i,j,k,3);
	return 0.5*ans/vol;
}

template <tw::Int C,tw::Int X,tw::Int Y,tw::Int Z>
inline tw::Float div(const Field& coeff,const Field& vf,tw::Int i,tw::Int j,tw::Int k,const MetricSpace& m)
{
	// divergence of a scalar field times a vector field
	tw::Float ans,vol;
	vol = m.dS(i,j,k,0);
	ans =  (coeff(i+1,j,k,C)+coeff(i,j,k,C))*(vf(i+1,j,k,X)+vf(i,j,k,X))*m.dS(i+1,j,k,1);
	ans -= (coeff(i-1,j,k,C)+coeff(i,j,k,C))*(vf(i-1,j,k,X)+vf(i,j,k,X))*m.dS(i,j,k,1);
	ans += (coeff(i,j+1,k,C)+coeff(i,j,k,C))*(vf(i,j+1,k,Y)+vf(i,j,k,Y))*m.dS(i,j+1,k,2);
	ans -= (coeff(i,j-1,k,C)+coeff(i,j,k,C))*(vf(i,j-1,k,Y)+vf(i,j,k,Y))*m.dS(i,j,k,2);
	ans += (coeff(i,j,k+1,C)+coeff(i,j,k,C))*(vf(i,j,k+1,Z)+vf(i,j,k,Z))*m.dS(i,j,k+1,3);
	ans -= (coeff(i,j,k-1,C)+coeff(i,j,k,C))*(vf(i,j,k-1,Z)+vf(i,j,k,Z))*m.dS(i,j,k,3);
	return 0.25*ans/vol;
}

template <tw::Int C,tw::Int S>
inline tw::Float div(const Field& coeff,const Field& sf,tw::Int i,tw::Int j,tw::Int k,const MetricSpace& m)
{
	// divergence of a scalar field times the gradient of another scalar field
	tw::Float ans,vol;
	vol = m.dS(i,j,k,0);
	ans =  (coeff(i+1,j,k,C)+coeff(i,j,k,C))*(sf(i+1,j,k,S)-sf(i,j,k,S))*m.dS(i+1,j,k,1)/m.dl(i+1,j,k,1);
	ans -= (coeff(i-1,j,k,C)+coeff(i,j,k,C))*(sf(i,j,k,S)-sf(i-1,j,k,S))*m.dS(i,j,k,1)/m.dl(i,j,k,1);
	ans += (coeff(i,j+1,k,C)+coeff(i,j,k,C))*(sf(i,j+1,k,S)-sf(i,j,k,S))*m.dS(i,j+1,k,2)/m.dl(i,j+1,k,2);
	ans -= (coeff(i,j-1,k,C)+coeff(i,j,k,C))*(sf(i,j,k,S)-sf(i,j-1,k,S))*m.dS(i,j,k,2)/m.dl(i,j,k,2);
	ans += (coeff(i,j,k+1,C)+coeff(i,j,k,C))*(sf(i,j,k+1,S)-sf(i,j,k,S))*m.dS(i,j,k+1,3)/m.dl(i,j,k+1,3);
	ans -= (coeff(i,j,k-1,C)+coeff(i,j,k,C))*(sf(i,j,k,S)-sf(i,j,k-1,S))*m.dS(i,j,k,3)/m.dl(i,j,k,3);
	return 0.5*ans/vol;
}

////////////////////////
//                    //
//  GATHER / SCATTER  //
//                    //
////////////////////////


inline void Field::Interpolate(std::valarray<tw::Float>& val,const Element& e,const weights_3D& weights)
{
	tw::Int i,j,k,s,ijk[4];
	tw::Float wijk;
	DecodeCell(weights.cell,ijk);
	val = 0.0;

	for (s=e.low;s<=e.high;s++)
		for (i=0;i<3;i++)
			for (j=0;j<3;j++)
				for (k=0;k<3;k++)
				{
					wijk = weights.w[i][0]*weights.w[j][1]*weights.w[k][2];
					val[s] += wijk*(*this)(ijk[1]+i-1,ijk[2]+j-1,ijk[3]+k-1,s);
				}
}

inline void Field::InterpolateOnto(std::valarray<tw::Float>& val,const Element& e,const weights_3D& weights)
{
	tw::Int i,j,k,s,ijk[4];
	tw::Float wijk;
	DecodeCell(weights.cell,ijk);

	for (s=e.low;s<=e.high;s++)
		for (i=0;i<3;i++)
			for (j=0;j<3;j++)
				for (k=0;k<3;k++)
				{
					wijk = weights.w[i][0]*weights.w[j][1]*weights.w[k][2];
					#pragma omp atomic update
					(*this)(ijk[1]+i-1,ijk[2]+j-1,ijk[3]+k-1,s) += wijk*val[s];
				}
}



//////////////////////////////
//                          //
//    AUTO FIELD TEMPLATE   //
//                          //
//////////////////////////////
//
// AutoField class may be used in the PARTICULAR case where the storage scheme
// is such that the components in a cell are stored contiguously in memory.
// The AutoField can then be used to treat the field as an array of type X,
// where X is some multi-component type that supports basic arithmetic operations.
// This is particularly useful for complex fields.

template <class T>
struct AutoField : Field
{
	std::valarray<tw::Float> elementArray; // for use as a temporary

	AutoField()
	{
		packedAxis = 0;
		num[0] = sizeof(T)/sizeof(tw::Float);
		elementArray.resize(num[0]);
	}
	void Initialize(const DiscreteSpace& ds,Task *task)
	{
		Field::Initialize(num[0],ds,task,tAxis);
	}

	// Interface to superclass accessors

	tw::Float& operator () (const tw::Int& x,const tw::Int& y,const tw::Int& z,const tw::Int& c)
		{ return Field::operator () (x,y,z,c); }
	tw::Float& operator () (const CellIterator& cell,const tw::Int& c)
		{ return Field::operator () (cell,c); }
	tw::Float& operator () (const StripIterator& strip,const tw::Int& x,const tw::Int& c)
		{ return Field::operator () (strip,x,c); }
	tw::Float& operator () (const VectorizingIterator<1>& v,const tw::Int& i,const tw::Int& c)
		{ return Field::operator () (v,i,c); }
	tw::Float& operator () (const VectorizingIterator<3>& v,const tw::Int& k,const tw::Int& c)
		{ return Field::operator () (v,k,c); }
	tw::Float operator () (const tw::Int& x,const tw::Int& y,const tw::Int& z,const tw::Int& c) const
		{ return Field::operator () (x,y,z,c); }
	tw::Float operator () (const CellIterator& cell,const tw::Int& c) const
		{ return Field::operator () (cell,c); }
	tw::Float operator () (const StripIterator& strip,const tw::Int& x,const tw::Int& c) const
		{ return Field::operator () (strip,x,c); }
	tw::Float operator () (const VectorizingIterator<1>& v,const tw::Int& i,const tw::Int& c) const
		{ return Field::operator () (v,i,c); }
	tw::Float operator () (const VectorizingIterator<3>& v,const tw::Int& k,const tw::Int& c) const
		{ return Field::operator () (v,k,c); }
	tw::Float operator () (const CellIterator& cell,const tw::Int& c,const tw::Int& ax) const
		{ return Field::operator () (cell,c,ax); }
	tw::Float operator () (const VectorizingIterator<1>& v,const tw::Int& i,const tw::Int& c,const tw::Int& ax) const
		{ return Field::operator () (v,i,c,ax); }
	tw::Float operator () (const VectorizingIterator<3>& v,const tw::Int& k,const tw::Int& c,const tw::Int& ax) const
		{ return Field::operator () (v,k,c,ax); }

	// Auto versions are distinguished by number of arguments.
	// f(i,j,k) or f(cell) or f(strip,i) call superclass and cast result to type T

	T& operator () (const tw::Int& x,const tw::Int& y,const tw::Int& z)
		{ return (T&)Field::operator () (x,y,z,0); }
	T& operator () (const CellIterator& cell)
		{ return (T&)Field::operator () (cell,0); }
	T& operator () (const StripIterator& strip,const tw::Int& x)
		{ return (T&)Field::operator () (strip,x,0); }
	T& operator () (const VectorizingIterator<1>& v,const tw::Int& i)
		{ return (T&)Field::operator () (v,i,0); }
	T& operator () (const VectorizingIterator<3>& v,const tw::Int& k)
		{ return (T&)Field::operator () (v,k,0); }

	void Interpolate(T* val,const weights_3D& weights)
	{
		tw::Int i;
		Field::Interpolate(elementArray,weights);
		for (i=0;i<num[0];i++)
			((tw::Float*)val)[i] = elementArray[i];
	}
	void InterpolateOnto(const T& val,const weights_3D& weights)
	{
		tw::Int i;
		for (i=0;i<num[0];i++)
			elementArray[i] = ((tw::Float*)&val)[i];
		Field::InterpolateOnto(elementArray,weights);
	}
	void AdjustTridiagonalForBoundaries(const axisSpec& axis,const sideSpec& side,T *T1,T *T2,T *T3,T *source,T val)
	{
		Field::AdjustTridiagonalForBoundaries(axis,side,(tw::Float*)T1,(tw::Float*)T2,(tw::Float*)T3,(tw::Float*)source,(tw::Float*)&val);
	}
	void Shift(const StripIterator& s,tw::Int cells,const T& incoming)
	{
		Field::Shift(s,cells,(tw::Float*)&incoming);
	}

	// Assignment

	AutoField<T>& operator = (T a)
	{
		for (CellIterator cell(*this,true);cell<cell.end();++cell)
			(*this)(cell) = a;
		return *this;
	}
	AutoField<T>& operator += (T a)
	{
		for (CellIterator cell(*this,true);cell<cell.end();++cell)
			(*this)(cell) += a;
		return *this;
	}
	AutoField<T>& operator -= (T a)
	{
		for (CellIterator cell(*this,true);cell<cell.end();++cell)
			(*this)(cell) -= a;
		return *this;
	}
	AutoField<T>& operator *= (T a)
	{
		for (CellIterator cell(*this,true);cell<cell.end();++cell)
			(*this)(cell) *= a;
		return *this;
	}
};



struct ScalarField:AutoField<tw::Float>
{
	tw::Float AxialEigenvalue(tw::Int z);
	tw::Float Eigenvalue(tw::Int x,tw::Int y);
	tw::Float CyclicEigenvalue(tw::Int x,tw::Int y);
	tw::Float CyclicEigenvalue(tw::Int x,tw::Int y,tw::Int z);

	void AxialSineTransform();
	void InverseAxialSineTransform();
	void TransverseSineTransform();
	void InverseTransverseSineTransform();
	void TransverseCosineTransform();
	void InverseTransverseCosineTransform();
	void TransverseFFT();
	void InverseTransverseFFT();

	// Assignment

	ScalarField& operator = (AutoField<tw::Float>& A)
	{
		return (ScalarField&)Field::operator=(A);
	}
	ScalarField& operator += (AutoField<tw::Float>& A)
	{
		return (ScalarField&)Field::operator+=(A);
	}
	ScalarField& operator -= (AutoField<tw::Float>& A)
	{
		return (ScalarField&)Field::operator-=(A);
	}
	ScalarField& operator *= (AutoField<tw::Float>& A)
	{
		return (ScalarField&)Field::operator*=(A);
	}
	ScalarField& operator = (tw::Float a)
	{
		return (ScalarField&)Field::operator=(a);
	}
	ScalarField& operator += (tw::Float a)
	{
		return (ScalarField&)Field::operator+=(a);
	}
	ScalarField& operator -= (tw::Float a)
	{
		return (ScalarField&)Field::operator-=(a);
	}
	ScalarField& operator *= (tw::Float a)
	{
		return (ScalarField&)Field::operator*=(a);
	}
};

struct ComplexField:AutoField<tw::Complex>
{
	tw::Float CyclicEigenvalue(tw::Int x,tw::Int y);
	tw::Float CyclicEigenvalue(tw::Int x,tw::Int y,tw::Int z);
	void FFT();
	void InverseFFT();
	void TransverseFFT();
	void InverseTransverseFFT();

	// Assignment

	ComplexField& operator = (AutoField<tw::Complex>& A)
	{
		return (ComplexField&)Field::operator=(A);
	}
	ComplexField& operator += (AutoField<tw::Complex>& A)
	{
		return (ComplexField&)Field::operator+=(A);
	}
	ComplexField& operator -= (AutoField<tw::Complex>& A)
	{
		return (ComplexField&)Field::operator-=(A);
	}
	ComplexField& operator *= (AutoField<tw::Complex>& A)
	{
		return (ComplexField&)Field::operator*=(A);
	}
	ComplexField& operator = (tw::Complex a)
	{
		return (ComplexField&)AutoField<tw::Complex>::operator=(a);
	}
	ComplexField& operator += (tw::Complex a)
	{
		return (ComplexField&)AutoField<tw::Complex>::operator+=(a);
	}
	ComplexField& operator -= (tw::Complex a)
	{
		return (ComplexField&)AutoField<tw::Complex>::operator-=(a);
	}
	ComplexField& operator *= (tw::Complex a)
	{
		return (ComplexField&)AutoField<tw::Complex>::operator*=(a);
	}
};

struct Vec3Field:AutoField<tw::vec3>
{
	//tw::Float CyclicEigenvalue(tw::Int x,tw::Int y);
	//void TransverseFFT();
	//void InverseTransverseFFT();

	// Assignment

	Vec3Field& operator = (AutoField<tw::vec3>& A)
	{
		return (Vec3Field&)Field::operator=(A);
	}
	Vec3Field& operator += (AutoField<tw::vec3>& A)
	{
		return (Vec3Field&)Field::operator+=(A);
	}
	Vec3Field& operator -= (AutoField<tw::vec3>& A)
	{
		return (Vec3Field&)Field::operator-=(A);
	}
	Vec3Field& operator *= (AutoField<tw::vec3>& A)
	{
		return (Vec3Field&)Field::operator*=(A);
	}
	Vec3Field& operator = (tw::vec3 a)
	{
		return (Vec3Field&)AutoField<tw::vec3>::operator=(a);
	}
	Vec3Field& operator += (tw::vec3 a)
	{
		return (Vec3Field&)AutoField<tw::vec3>::operator+=(a);
	}
	Vec3Field& operator -= (tw::vec3 a)
	{
		return (Vec3Field&)AutoField<tw::vec3>::operator-=(a);
	}
	Vec3Field& operator *= (tw::vec3 a)
	{
		return (Vec3Field&)AutoField<tw::vec3>::operator*=(a);
	}
};

////////////////////
//                //
// Slice Template //
//                //
////////////////////

template <class T>
Slice<T>::Slice(const Element& e,tw::Int xl,tw::Int xh,tw::Int yl,tw::Int yh,tw::Int zl,tw::Int zh)
{
	tw::Int low[4] = { 0 , xl , yl , zl };
	tw::Int high[4] = { 0 , xh , yh , zh };
	tw::Int ignorable[4] = { 0 , 0 , 0 , 0 };
	Resize(e,low,high,ignorable);
}

template <class T>
Slice<T>::Slice(const Element& e,tw::Int low[4],tw::Int high[4],tw::Int ignorable[4])
{
	Resize(e,low,high,ignorable);
}

template <class T>
void Slice<T>::Resize(const Element& e,tw::Int low[4],tw::Int high[4],tw::Int ignorable[4])
{
	this->e = e;
	lb[1] = low[1];
	ub[1] = high[1];
	lb[2] = low[2];
	ub[2] = high[2];
	lb[3] = low[3];
	ub[3] = high[3];
	// Decoding strides, no zero strides
	decodingStride[3] = e.Components();
	decodingStride[2] = decodingStride[3]*(ub[3]-lb[3]+1);
	decodingStride[1] = decodingStride[2]*(ub[2]-lb[2]+1);
	// Encoding strides, zero strides used
	encodingStride[1] = decodingStride[1] * (1 - ignorable[1]);
	encodingStride[2] = decodingStride[2] * (1 - ignorable[2]);
	encodingStride[3] = decodingStride[3] * (1 - ignorable[3]);
	// Allocate space
	data.resize(e.Components()*(ub[1]-lb[1]+1)*(ub[2]-lb[2]+1)*(ub[3]-lb[3]+1));
}

template <class T>
void Slice<T>::Translate(const axisSpec& axis,tw::Int displ)
{
	tw::Int ax = naxis(axis);
	lb[ax] += displ;
	ub[ax] += displ;
}

template <class T>
void Slice<T>::Translate(tw::Int x,tw::Int y,tw::Int z)
{
	lb[1] += x;
	ub[1] += x;
	lb[2] += y;
	ub[2] += y;
	lb[3] += z;
	ub[3] += z;
}

template <class T>
void Field::LoadDataIntoImage(Slice<T>* v)
{
	tw::Int i,j,k,s;
	for (s=v->e.low;s<=v->e.high;s++)
		for (i=v->lb[1];i<=v->ub[1];i++)
			for (j=v->lb[2];j<=v->ub[2];j++)
				for (k=v->lb[3];k<=v->ub[3];k++)
					(*v)(i,j,k,s) = (*this)(i,j,k,s);
}

template <class T>
void Field::SaveDataFromImage(Slice<T>* v)
{
	tw::Int i,j,k,s;
	for (s=v->e.low;s<=v->e.high;s++)
		for (i=v->lb[1];i<=v->ub[1];i++)
			for (j=v->lb[2];j<=v->ub[2];j++)
				for (k=v->lb[3];k<=v->ub[3];k++)
					(*this)(i,j,k,s) = (*v)(i,j,k,s);
}

template <class T>
void Field::ZeroDataInField(Slice<T>* v)
{
	tw::Int i,j,k,s;
	for (s=v->e.low;s<=v->e.high;s++)
		for (i=v->lb[1];i<=v->ub[1];i++)
			for (j=v->lb[2];j<=v->ub[2];j++)
				for (k=v->lb[3];k<=v->ub[3];k++)
					(*this)(i,j,k,s) = 0.0;
}

template <class T>
void Field::AddDataFromImage(Slice<T>* v)
{
	tw::Int i,j,k,s;
	for (s=v->e.low;s<=v->e.high;s++)
		for (i=v->lb[1];i<=v->ub[1];i++)
			for (j=v->lb[2];j<=v->ub[2];j++)
				for (k=v->lb[3];k<=v->ub[3];k++)
					(*this)(i,j,k,s) += (*v)(i,j,k,s);
}

template <class T>
void Field::AddDataFromImageAtomic(Slice<T>* v)
{
	tw::Int i,j,k,s;
	for (s=v->e.low;s<=v->e.high;s++)
		for (i=v->lb[1];i<=v->ub[1];i++)
			for (j=v->lb[2];j<=v->ub[2];j++)
				for (k=v->lb[3];k<=v->ub[3];k++)
					#pragma omp atomic update
					(*this)(i,j,k,s) += (*v)(i,j,k,s);
}
