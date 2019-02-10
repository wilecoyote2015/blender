#include "graph_to_function.hpp"
#include "cpu.hpp"

namespace FN {

	class ExecuteGraph : public TupleCallBody {
	private:
		const SharedDataFlowGraph m_graph;
		const SmallSocketSetVector m_inputs;
		const SmallSocketSetVector m_outputs;

	public:
		ExecuteGraph(
			const SharedDataFlowGraph &graph,
			const SmallSocketVector &inputs,
			const SmallSocketVector &outputs)
			: m_graph(graph), m_inputs(inputs), m_outputs(outputs) {}

		void call(const Tuple &fn_in, Tuple &fn_out) const override
		{
			for (uint i = 0; i < m_outputs.size(); i++) {
				this->compute_socket(fn_in, fn_out, i, m_outputs[i]);
			}
		}

		void compute_socket(const Tuple &fn_in, Tuple &out, uint out_index, Socket socket) const
		{
			if (m_inputs.contains(socket)) {
				uint index = m_inputs.index(socket);
				Tuple::copy_element(fn_in, index, out, out_index);
			}
			else if (socket.is_input()) {
				this->compute_socket(fn_in, out, out_index, socket.origin());
			}
			else {
				const Node *node = socket.node();
				const Signature &signature = node->signature();

				Tuple tmp_in(signature.input_types());
				Tuple tmp_out(signature.output_types());

				for (uint i = 0; i < signature.inputs().size(); i++) {
					this->compute_socket(fn_in, tmp_in, i, node->input(i));
				}

				const TupleCallBody *body = node->function()->body<TupleCallBody>();
				body->call(tmp_in, tmp_out);

				Tuple::copy_element(tmp_out, socket.index(), out, out_index);
			}
		}
	};

	static Signature signature_from_sockets(
		const SmallSocketVector &input_sockets,
		const SmallSocketVector &output_sockets)
	{
		InputParameters inputs;
		OutputParameters outputs;

		for (const Socket &socket : input_sockets) {
			inputs.append(InputParameter(socket.name(), socket.type()));
		}
		for (const Socket &socket : output_sockets) {
			outputs.append(OutputParameter(socket.name(), socket.type()));
		}

		return Signature(inputs, outputs);
	}

	SharedFunction function_from_data_flow(
		const SharedDataFlowGraph &graph,
		const SmallSocketVector &inputs,
		const SmallSocketVector &outputs)
	{
		Signature signature = signature_from_sockets(inputs, outputs);
		SharedFunction fn = SharedFunction::New(signature);
		fn->add_body<TupleCallBody>(new ExecuteGraph(graph, inputs, outputs));
		return fn;
	}

} /* namespace FN */