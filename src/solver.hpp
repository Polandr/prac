#include <complex>
#include <cmath>

#define DEFAULT_H_FILE "Matrix_H"
#define DEFAULT_R0_FILE "Matrix_R0"
#define DEFAULT_DT 0.1
#define DEFAULT_STEP_NUM 1

using namespace std;

// Solver exception class-----------------------------------------------------------------

class Solver_exception: public std::exception
{
	mutable char* errstr; 

	public:

	Solver_exception(const char* str = "")
	{
		errstr = const_cast <char*> (str);
	}
	~Solver_exception() throw()
	{
		delete [] errstr;
	}
	virtual const char* what() const throw()
	{
		char* tmp = errstr;
		char* prefix = const_cast <char*> ("Solver class error: ");
		try
		{
			errstr = new char [strlen(prefix)+strlen(errstr)+2];
		}
		catch (std::exception& smth)
		{
			return "Couldn't generate an error message (there is no memory)\n";
		}
		sprintf(errstr, "%s%s.\n", prefix, tmp);
		return errstr;
	}	
};

// Service functions:

int combination_num(int k, int n)
{
	int out = 1;
	for (int i = k+1; i <= n; i++)
		out *= i;
	for (int i = 1; i <= n-k; i++)
		out /= i;
	return out;
}

int unit_num(int state, int N)
{
	int mask = 1, out = 0;
	for (int i = 0; i < N; i++)
	{
		if (mask & i != 0)
			out++;
		mask = mask << 1;
	}
	return out;
}

void collect_base_states (int E_lvl, int N, vector<int>& base_states)
{
	int mask = 1;
	int full_energy_state = 0;
	for (int i = 0; i < N; i++)
	{
		full_energy_state = full_energy_state | mask;
		mask = mask << 1;
	}
	vector<int> cur_base_states;
	int pow_N = pow(2,N);
	for (int i = 0; i < pow_N; i++)
	{
		if (unit_num(i,N) == E_lvl)
			cur_base_states.push_back(i);
	}
	base_states.insert(base_states.end(),cur_base_states.begin(),cur_base_states.end());
}

int simple_transition(int state_1, int state_2, int N)
{
	int out = -1;
	int state_sum = state_1 ^ state_2;
	int prev = 0;
	bool found = false;
	for (int i = 0, mask = 1; i < N; i++)
	{
		if (mask & state_sum != 0)
			if (!found)
				if (prev == 0)
				{
					if (mask & state_1 != 0)
						prev = 1;
					if (mask & state_2 != 0)
						prev = 2;
				}
				else
				{
					if (prev == 1 && (mask & state_2 != 0))
					{
						prev = 0;
						found = true;
						out = i-1;
					}
					if (prev == 2 && (mask & state_1 != 0))
					{
						prev = 0;
						found = true;
						out = i-1;
					}
				}
			else
				out = -1;
		else
			prev = 0;
		mask = mask << 1;
	}
	return out;
}

complexd hamiltonian_element(int row, int col, int N, vector<complexd> a, vector<complexd> w, vector<int> states)
{
	if (row == col)
	{
		complexd out(0,0);
		for (int i = 0, mask = 1; i < N; i++)
		{
			if (mask & states[row] != 0)
				out += w[i];
			mask = mask << 1;
		}
		return out;
	}
	else
	{
		int pos = simple_transition(states[row],states[col],N);
		if (pos != -1)
			return a[pos];
		else
			return complexd(0,0);
	}
}

// Solver functions realization------------------------------------------------------------

// Hamiltonian initialization:

void Solver::init_hamiltonian (const char* filename = DEFAULT_H_FILE)
{
	H.readf(filename);
	if (!(H.is_square()))
		throw Solver_exception("incorrect matrix dimensions in hamiltonian");
}

void Solver::init_hamiltonian (const Matrix& matrix_H)
{
	H = matrix_H;
	if (!(H.is_square()))
		throw Solver_exception("incorrect matrix dimensions in hamiltonian");
}

void Solver::init_hamiltonian (int N, int s, int E_min, int E_max, vector<complexd> a, vector<complexd> w)
{
	if (a.size() != N-1 || w.size() != N)
		throw Solver_exception("incorrect parameters in hamiltonian initialization");
	s = min(s,E_max);
	vector<int> base_states;
	int E_lowest = max(0,E_min-s);

	for (int i = 0; i <= s; i++)
	{
		int low = max(0,E_min-s);
		int high = max(0,E_max-s);
		for (int E_lvl = low; E_lvl <= high; E_lvl++)
			collect_base_states(E_lvl,N,base_states);
	}

	H.init(base_states.size(),base_states.size());
	for (int i = 0; i < base_states.size(); i++)
		for (int j = i; j < base_states.size(); j++)
		{
			complexd val = hamiltonian_element(i,j,N,a,w,base_states);
			H.set(i,j,val);
			if (i != j)
				H.set(j,i,conj(val));
		}
}

// Initial density matrix initialization:

void Solver::init_density_matrix (const char* filename = DEFAULT_R0_FILE)
{
	R0.readf(filename);
	if (!(R0.is_square()))
		throw Solver_exception("incorrect matrix dimensions in initital density matrix");
}

void Solver::init_density_matrix (const Matrix& matrix_R0)
{
	R0 = matrix_R0;
	if (!(R0.is_square()))
		throw Solver_exception("incorrect matrix dimensions in initital density matrix");
}

void Solver::init_density_matrix (vector<complexd> state)
{
	R0.init_density_matrix(state);
}

void Solver::init_time_step (double dt = DEFAULT_DT)
{
	dT = dt;
}

void Solver::init_step_num (int steps = DEFAULT_STEP_NUM)
{
	step_num = steps;
}

void Solver::init_system ()
{
	init_hamiltonian(DEFAULT_H_FILE);
	init_density_matrix(DEFAULT_R0_FILE);
	init_time_step(DEFAULT_DT);
	init_step_num(DEFAULT_STEP_NUM);
}

void print_header (FILE* file)
{
	if (ProcessorGrid::is_root())
		fprintf(file, "Magnitudes of diagonal elements are:\n");
}

void Solver::solve (const char* filename)
{
	FILE* file;
	if (filename != NULL)
	{
		file = fopen(filename,"w");
		print_header(file);
	}
	else
		print_header(stdout);

	Matrix U = exp(H, dT); 
	Matrix U_c = ~U;
	Matrix Rt = R0;

	for (int i = 0; i < step_num; i++)
	{
		Rt = U_c*Rt; 
		Rt = Rt*U; 
		if (filename != NULL)
			Rt.print_diagonal_abs(file);
		else
			Rt.print_diagonal_abs(stdout);
	}
	if (filename != NULL)
		fclose(file);
}

ostream& operator << (ostream& out, Solver& src)
{
	out << "System configuration is:\n";
	out << "Matrix H:\n" << src.get_hamiltonian() << endl;
	out << "Matrix R0:\n" << src.get_density_matrix() << endl;
	out << "dT: " << src.get_time_step() << endl;
	out << "step number: " << src.get_step_num() << endl;

	return out;
}

istream& operator >> (istream& in, Solver& trg)
{
	in >> trg.get_hamiltonian();
	in >> trg.get_density_matrix();
	in >> trg.get_time_step();
	in >> trg.get_step_num();

	return in;
}
