#ifndef _COMMON_HPDDM_
#define _COMMON_HPDDM_

#include <math.h>
#include <mpi.h>
#include <ff++.hpp>
#include <AFunction_ext.hpp>
#include <array_tlp.hpp>

#ifdef WITH_mkl
#define HPDDM_MKL 1
#define MKL_Complex8 std::complex<float>
#define MKL_Complex16 std::complex<double>
#include <mkl.h>
#endif

#if HPDDM_SCHWARZ || HPDDM_FETI || HPDDM_BDD
#ifdef WITH_mkl
#define MKL_PARDISOSUB
#elif defined(WITH_mumps)
#define MUMPSSUB
#else
#define SUITESPARSESUB
#endif

#ifdef WITH_mumps
#define DMUMPS
#else
#define DSUITESPARSE
#endif
#define MU_ARPACK
#endif

#define HPDDM_NUMBERING 'C'
#undef CBLAS_H

#include <HPDDM.hpp>

#include "common.hpp"

template<class K> K* newCopy(bool mfree,K *p,int n)
{  if( !mfree) return p;
	K *q= new K[n];
    copy(p,p+n,q);
    return q;
}
template<class K>  using MatriceMorse=HashMatrix<int,K>;

template<class K>
struct ff_HPDDM_MatrixCSR : public HPDDM::MatrixCSR<K>
{
    ff_HPDDM_MatrixCSR(MatriceMorse<K>* mA) :
    HPDDM::MatrixCSR<K>(mA->n, mA->m, mA->nnz, mA->aij, mA->p, mA->j, mA->half) {
        mA->CSR();
        this->_ia=mA->p;
        // PB delete FH?????
    }
};

template<class K>
HPDDM::MatrixCSR<K> * new_HPDDM_MatrixCSR(MatriceMorse<K   >* mA,bool mfree=false,K *s=0,int *is=0,int *js=0)
{ if(mA)
    {
        int nnz = mA->nnz, n = mA->n;
        mA->CSR();
        if(!s) s=newCopy(mfree,mA->aij,nnz);
        if(!is) is=newCopy(mfree,mA->p,n+1);
        if(!js) js=newCopy(mfree,mA->j,nnz);

        return new HPDDM::MatrixCSR<K>(mA->n, mA->m, mA->nnz, s, is, js , mA->half,mfree);
    }
    else
        return 0;
}
template<class K>
HPDDM::MatrixCSR<void> * new_HPDDM_MatrixCSRvoid(MatriceMorse<K   >* mA,bool mfree=false,int *is=0,int *js=0)
{ if(mA)
{
    mA->CSR();
    if(!js) js=mA->j;
    if(!is) is=mA->p;
    return new HPDDM::MatrixCSR<void>(mA->n, mA->m, mA->nnz, is, js , mA->half,mfree);
}
else
    return 0;
}

template<class K>
void set_ff_matrix(MatriceMorse<K>* mA,const HPDDM::MatrixCSR<K> &dA)
{
    //void HashMatrix<I,R>::set(I nn,I mm,bool hhalf,size_t nnnz, I *ii, I*jj, R *aa,,int f77,int tcsr)
    if(verbosity>99) cout << " set_ff_matrix " <<endl;
    // Warning this pointeur a change or not in hpddm => not del in HashMatrix
    mA->j=0;
    mA->p=0;
    mA->aij=0;
    
    mA->set(dA._n,dA._m,dA._sym,dA._nnz,dA._ia,dA._ja,dA._a,0,1);
}

template<typename T>
inline bool exist_type() {
    map<const string,basicForEachType*>::iterator ir = map_type.find(typeid(T).name());
    return ir != map_type.end();
}

template<class T>
class STL {
    T* const _it;
    const int _size;
    public:
        STL(const KN<T>& v) : _it(v), _size(v.size()) { };
        int size() const {
            return _size;
        }
        T* begin() const {
            return _it;
        }
        T* end() const {
            return _it + _size;
        }
        bool empty() const { return _size <= 0; }
        T& operator[](std::size_t idx) { return _it[idx]; }
        const T& operator[](std::size_t idx) const { return _it[idx]; }
        T& back() { return _it[_size - 1]; }
        const T& back() const { return _it[_size - 1]; }
};
template<class K>
class Pair {
    public:
        Pair() : p() { };
        std::pair<MPI_Request, const K*>* p;
        void init() { }
        void destroy() {
            delete p;
            p = nullptr;
        }
};

extern KN<String>* pkarg;

template<class Type, class K, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value == 1>::type* = nullptr>
void exchange(Type* const& pA, K* pin, unsigned short mu, bool allocate) {
    if(allocate)
        pA->template exchange<true>(pin, mu);
    else
        pA->template exchange<false>(pin, mu);
}
template<class Type, class K, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value != 1>::type* = nullptr>
void exchange(Type* const& pA, K* pin, unsigned short mu, bool allocate) { }
template<class Type, class K>
void exchange_dispatched(Type* const& pA, KN<K>* pin, bool scaled) {
    if(pA) {
        unsigned short mu = pA->getDof() ? pin->n / pA->getDof() : 1;
        const auto& map = pA->getMap();
        bool allocate = map.size() > 0 && pA->getBuffer()[0] == nullptr ? pA->setBuffer() : false;
        if(scaled)
            exchange(pA, static_cast<K*>(*pin), mu, false);
        else
            pA->HPDDM::template Subdomain<K>::exchange(static_cast<K*>(*pin), mu);
        pA->clearBuffer(allocate);
    }
}
template<class Type, class K, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value != 0>::type* = nullptr>
void exchange(Type* const& pA, KN<K>* pin, bool scaled) {
    exchange_dispatched(pA, pin, scaled);
}
template<class Type, class K, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value == 0>::type* = nullptr>
void exchange(Type* const& pA, KN<K>* pin, bool scaled) {
    if(pA)
        exchange_dispatched(pA->_A, pin, scaled);
}
template<class Type, class K, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value != 0>::type* = nullptr>
void exchange_restriction(Type* const&, KN<K>*, KN<K>*, MatriceMorse<double>*) { }
namespace PETSc {
template<class Type, class K>
    void changeNumbering_func(Type*, KN<K>*, KN<K>*, bool){ ffassert(0);} // Modif FH. Missing function Do Day
}
template<class Type, class K, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value == 0>::type* = nullptr>
void exchange_restriction(Type* const& pA, KN<K>* pin, KN<K>* pout, MatriceMorse<double>* mR) {
    if(pA->_exchange && !pA->_exchange[1]) {
        ffassert((!mR && pA->_exchange[0]->getDof() == pout->n) || (mR && mR->n == pin->n && mR->m == pout->n));
        PETSc::changeNumbering_func(pA, pin, pout, false);
        PETSc::changeNumbering_func(pA, pin, pout, true);
        pout->resize(pA->_exchange[0]->getDof());
        *pout = K();

        if(mR) {
  //          mR->addMatTransMul(*pin,*pout);;
  //  out += A^t in
#ifndef VERSION_MATRICE_CREUSE
            for(int i = 0; i < mR->n; ++i) {
                for(int j = mR->lg[i]; j < mR->lg[i + 1]; ++j)
                    pout->operator[](mR->cl[j]) += mR->a[j] * pin->operator[](i);
            }
#else
            for(int k = 0; k < mR->nnz; ++k)
                    pout->operator[](mR->j[k]) += mR->aij[k] * pin->operator[](mR->i[k]);
#endif
        }
    
        exchange_dispatched(pA->_exchange[0], pout, false);
    }
}
template<class Type, class K>
class exchangeIn_Op : public E_F0mps {
    public:
        Expression A;
        Expression in;
        static const int n_name_param = 1;
        static basicAC_F0::name_and_type name_param[];
        Expression nargs[n_name_param];
        exchangeIn_Op<Type, K>(const basicAC_F0& args, Expression param1, Expression param2) : A(param1), in(param2) {
            args.SetNameParam(n_name_param, name_param, nargs);
        }

        AnyType operator()(Stack stack) const;
};
template<class Type, class K>
basicAC_F0::name_and_type exchangeIn_Op<Type, K>::name_param[] = {
    {"scaled", &typeid(bool)}
};
template<class Type, class K>
class exchangeIn : public OneOperator {
    public:
        exchangeIn() : OneOperator(atype<long>(), atype<Type*>(), atype<KN<K>*>()) { }

        E_F0* code(const basicAC_F0& args) const {
            return new exchangeIn_Op<Type, K>(args, t[0]->CastTo(args[0]), t[1]->CastTo(args[1]));
        }
};
template<class Type, class K>
AnyType exchangeIn_Op<Type, K>::operator()(Stack stack) const {
    Type* pA = GetAny<Type*>((*A)(stack));
    KN<K>* pin = GetAny<KN<K>*>((*in)(stack));
    const bool scaled = mpisize > 1 && nargs[0] && GetAny<bool>((*nargs[0])(stack));
    exchange(pA, pin, scaled);
    return 0L;
}
template<class Type, class K>
class exchangeInOut_Op : public E_F0mps {
    public:
        Expression A;
        Expression in;
        Expression out;
        static const int n_name_param = 2;
        static basicAC_F0::name_and_type name_param[];
        Expression nargs[n_name_param];
        exchangeInOut_Op<Type, K>(const basicAC_F0& args, Expression param1, Expression param2, Expression param3) : A(param1), in(param2), out(param3) {
            args.SetNameParam(n_name_param, name_param, nargs);
        }

        AnyType operator()(Stack stack) const;
};
template<class Type, class K>
basicAC_F0::name_and_type exchangeInOut_Op<Type, K>::name_param[] = {
    {"scaled", &typeid(bool)},
    {"restriction", &typeid(Matrice_Creuse<double>*)}
};
template<class Type, class K>
class exchangeInOut : public OneOperator {
    public:
        exchangeInOut() : OneOperator(atype<long>(), atype<Type*>(), atype<KN<K>*>(), atype<KN<K>*>()) { }

        E_F0* code(const basicAC_F0& args) const {
            return new exchangeInOut_Op<Type, K>(args, t[0]->CastTo(args[0]), t[1]->CastTo(args[1]), t[2]->CastTo(args[2]));
        }
};
template<class Type, class K>
AnyType exchangeInOut_Op<Type, K>::operator()(Stack stack) const {
    Type* pA = GetAny<Type*>((*A)(stack));
    KN<K>* pin = GetAny<KN<K>*>((*in)(stack));
    KN<K>* pout = GetAny<KN<K>*>((*out)(stack));
    const bool scaled = mpisize > 1 && nargs[0] && GetAny<bool>((*nargs[0])(stack));
    Matrice_Creuse<double>* pR = nargs[1] ? GetAny<Matrice_Creuse<double>*>((*nargs[1])(stack)) : nullptr;
    MatriceMorse<double>* mR = pR ? static_cast<MatriceMorse<double>*>(&(*pR->A)) : nullptr;
    if(pR) {
        ffassert(!scaled);
        exchange_restriction(pA, pin, pout, mR);
    }
    else if(pin->n == pout->n) {
        *pout = *pin;
        exchange(pA, pout, scaled);
    }
    return 0L;
}

#if !HPDDM_PETSC || !defined(HPDDM_SLEPC)
double getOpt(string* const& ss) {
    return HPDDM::Option::get()->val(*ss);
}
bool isSetOpt(string* const& ss) {
    return HPDDM::Option::get()->set(*ss);
}
#endif
template<class Type, class K>
bool destroyRecycling(Type* const& Op) {
#ifndef HPDDM_SLEPC
    HPDDM::Recycling<K>::get()->destroy(Op->prefix());
#else
    Op->destroy();
#endif
    return false;
}
template<class Type>
bool statistics(Type* const& Op) {
    Op->statistics();
    return false;
}

template<class K, class Type, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value == 1>::type* = nullptr>
const K* getScaling(Type* const& pA) {
    if(pA)
        return pA->getScaling();
    else
        return nullptr;
}
template<class K, class Type, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value != 1>::type* = nullptr>
const K* getScaling(Type* const& pA) {
    if(pA)
        return getScaling<K>(pA->_A);
    else
        return nullptr;
}
template<class Type, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value == 1>::type* = nullptr>
MPI_Comm getCommunicator(Type* const& pA) {
    if(pA)
        return pA->getCommunicator();
    else
        return MPI_COMM_WORLD;
}
template<class Type, typename std::enable_if<HPDDM::hpddm_method_id<Type>::value != 1>::type* = nullptr>
MPI_Comm getCommunicator(Type* const& pA) {
    if(pA)
        return getCommunicator(pA->_A);
    else
        return MPI_COMM_WORLD;
}
template<class K, typename std::enable_if<!std::is_same<K, double>::value>::type* = nullptr>
inline K prod(K u, double d, K v) {
    return std::conj(u) * d * v;
}
template<class K, typename std::enable_if<std::is_same<K, double>::value>::type* = nullptr>
inline K prod(K u, double d, K v) {
    return u * d * v;
}
template<class Type, class K>
K scalarProduct(Type* const& Op, KN<K>* const& u, KN<K>* const& v) {
    const HPDDM::underlying_type<K>* const d = getScaling<HPDDM::underlying_type<K>>(Op);
    MPI_Comm comm = getCommunicator(Op);
    K val = 0;
    if(d) {
        for(int i = 0; i < u->n; ++i)
            val += prod(u->operator[](i), d[i], v->operator[](i));
    }
    else
        val = (*u, *v);
    MPI_Allreduce(MPI_IN_PLACE, &val, 1, HPDDM::Wrapper<K>::mpi_type(), MPI_SUM, comm);
    return val;
}
template<class Op, class K>
void addScalarProduct() {
    atype<Op*>()->Add("(", "", new OneOperator3_<K, Op*, KN<K>*, KN<K>*>(scalarProduct<Op, K>));
}

template<class A>
inline AnyType MyDestroyKN(Stack, const AnyType& x) {
    KN<A>* a = GetAny<KN<A>*>(x);
    for(int i = 0; i < a->N(); ++i)
        (*a)[i].dtor();
    a->destroy();
    return Nothing;
}
template<class R>
R* InitKN(R* const& a, const long& n) {
    a->init(n);
    return a;
}
template<class T>
T* resizeClean(const Resize<T>& t, const long &n) {
    int m = t.v->N();
    for(int i = n; i < m; ++i)
        (*t.v)[i].dtor();
    t.v->resize(n);
    return t.v;
}

template<class Op>
void addArray() {
    Dcl_Type<KN<Op>*>(0, MyDestroyKN<Op>);
    TheOperators->Add("<-", new OneOperator2_<KN<Op>*, KN<Op>*, long>(&InitKN));
    atype<KN<Op>*>()->Add("[", "", new OneOperator2_<Op*, KN<Op>*, long>(get_elementp_<Op, KN<Op>*, long>));
    Dcl_Type<Resize<KN<Op>>>();
    Add<KN<Op>*>("resize", ".", new OneOperator1<Resize<KN<Op>>, KN<Op>*>(to_Resize));
    Add<Resize<KN<Op>>>("(", "", new OneOperator2_<KN<Op>*, Resize<KN<Op>>, long>(resizeClean));
    map_type_of_map[make_pair(atype<long>(), atype<Op*>())] = atype<KN<Op>*>();
}

void parallelIO(string*& name, MPI_Comm* const& comm, bool const& append) {
    std::string base_filename(*name);
    std::string::size_type p(base_filename.find_last_of('.'));
    std::string file_without_extension = base_filename.substr(0, p);
    std::string extension;
    if(p == std::string::npos)
        extension = "vtu";
    else
        extension = base_filename.substr(p + 1, std::string::npos);
    p = base_filename.find_last_of("/\\");
    if(p == std::string::npos)
        base_filename = file_without_extension;
    else
        base_filename = file_without_extension.substr(p + 1, std::string::npos);
    int rank;
    int size;
    MPI_Comm_rank(comm ? *comm : MPI_COMM_WORLD, &rank);
    MPI_Comm_size(comm ? *comm : MPI_COMM_WORLD, &size);
    std::ostringstream str[3];
    str[2] << size;
    str[1] << std::setw(str[2].str().length()) << std::setfill('0') << rank;
    ofstream pvd;
    int T = 0;
    if(append) {
        if(rank == 0) {
            ifstream input;
            input.open(file_without_extension + (size > 1 ? "_" + str[2].str() : "") + ".pvd");
            if(input.peek() != std::ifstream::traits_type::eof()) {
                std::string line;
                std::getline(input, line);
                std::getline(input, line);
                std::string delimiter = "\"";
                p = line.find(delimiter);
                line = line.substr(p + 1, std::string::npos);
                p = line.find(delimiter);
                T = std::stoi(line.substr(0, p)) + 1;
            }
        }
        MPI_Bcast(&T, 1, MPI_INT, 0, comm ? *comm : MPI_COMM_WORLD);
    }
    str[0] << std::setw(4) << std::setfill('0') << T;
    *name = file_without_extension + "_" + (size > 1 ? str[2].str() + "_" : "") + str[0].str() + (size > 1 ? "_" + str[1].str() : "") + "." + extension;
    if(rank == 0) {
        pvd.open(file_without_extension + (size > 1 ? "_" + str[2].str() : "") + ".pvd");
        pvd << "<?xml version=\"1.0\"?>\n";
        pvd << "<VTKFile T=\"" << T << "\" type=\"Collection\" version=\"0.1\"\n";
        pvd << "         byte_order=\"LittleEndian\"\n";
        pvd << "         compressor=\"vtkZLibDataCompressor\">\n";
        pvd << "  <Collection>\n";
        for(int t = 0; t < T + 1; ++t)
            for(int i = 0; i < size; ++i) {
                pvd << "    <DataSet timestep=\"" << t << "\" group=\"\" part=\"" << i << "\"\n";
                pvd << "             file=\"";
                pvd << base_filename << "_";
                if(size > 1) pvd << str[2].str() + "_";
                pvd << std::setw(4) << std::setfill('0') << t;
                if(size > 1) pvd << "_" << std::setw(str[2].str().length()) << std::setfill('0') << i;
                pvd << "." << std::setw(0) << extension << "\"/>\n";
            }
        pvd << "  </Collection>\n";
        pvd << "</VTKFile>\n";
    }
}

#define COMMON_HPDDM_PARALLEL_IO
#include "../seq/iovtk.cpp"

#if defined(PETSCSUB) || HPDDM_PETSC
namespace PETSc {
  void finalizePETSc( ) {
    PETSC_COMM_WORLD = MPI_COMM_WORLD;
    PetscBool isFinalized;
    PetscFinalized(&isFinalized);
    if (!isFinalized) PetscFinalize( );
  }
}
#endif

static void Init_Common() {
    if(!Global.Find("savevtk").NotNull()) {
        Global.Add("savevtk", "(", new OneOperatorCode<VTK_WriteMesh_Op> );
        Global.Add("savevtk", "(", new OneOperatorCode<VTK_WriteMesh3_Op> );
    }
#if HPDDM_SCHWARZ || HPDDM_FETI || HPDDM_BDD
    aType t;
    int r;
    if(!zzzfff->InMotClef("pair", t, r)) {
        Global.Add("getOption", "(", new OneOperator1_<double, string*>(getOpt));
        Global.Add("isSetOption", "(", new OneOperator1_<bool, string*>(isSetOpt));
        int argc = pkarg->n;
        const char** argv = new const char*[argc];
        for(int i = 0; i < argc; ++i)
            argv[i] = (*((*pkarg)[i].getap()))->data();
        HPDDM::Option::get()->parse(argc, argv, mpirank == 0);
#ifdef PETSCSUB
        PetscInitialize(&argc, const_cast<char***>(&argv), 0, "");
        ff_atend(PETSc::finalizePETSc);
#endif
        delete [] argv;
    }
#endif
}
#endif // _COMMON_HPDDM_
