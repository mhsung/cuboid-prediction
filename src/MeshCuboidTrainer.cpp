#include "MeshCuboidTrainer.h"
#include "Utilities.h"

#include <Eigen/Core>
#include <Eigen/LU>
#include <QFileInfo>

#include <deque>
#include <fstream>
#include <iostream>
#include <sstream>

Eigen::MatrixXd regularized_inverse(const Eigen::MatrixXd& _mat)
{
	return (_mat + 1.0E-3 * Eigen::MatrixXd::Identity(_mat.rows(), _mat.cols())).inverse();
}

MeshCuboidTrainer::MeshCuboidTrainer()
{

}

void MeshCuboidTrainer::clear()
{
	for (std::vector< std::list<MeshCuboidFeatures *> >::iterator f_it = feature_list_.begin();
		f_it != feature_list_.end(); ++f_it)
	{
		for (std::list<MeshCuboidFeatures *>::iterator f_jt = (*f_it).begin();
			f_jt != (*f_it).end(); ++f_jt)
			delete (*f_jt);
		(*f_it).clear();
	}
	feature_list_.clear();

	for (std::vector< std::list<MeshCuboidTransformation *> >::iterator t_it = transformation_list_.begin();
		t_it != transformation_list_.end(); ++t_it)
	{
		for (std::list<MeshCuboidTransformation *>::iterator t_jt = (*t_it).begin();
			t_jt != (*t_it).end(); ++t_jt)
			delete (*t_jt);
		(*t_it).clear();
	}
	transformation_list_.clear();
}

bool MeshCuboidTrainer::load_object_list(const std::string &_filename)
{
	std::ifstream file(_filename);
	if (!file)
	{
		std::cerr << "Can't load file: \"" << _filename << "\"" << std::endl;
		return false;
	}

	object_list_.clear();
	std::string buffer;

	while (!file.eof())
	{
		std::getline(file, buffer);
		if (buffer == "") break;
		object_list_.push_back(buffer);
	}

	return true;
}

bool MeshCuboidTrainer::load_features(const std::string &_filename_prefix)
{
	for (std::vector< std::list<MeshCuboidFeatures *> >::iterator f_it = feature_list_.begin();
		f_it != feature_list_.end(); ++f_it)
	{
		for (std::list<MeshCuboidFeatures *>::iterator f_jt = (*f_it).begin();
			f_jt != (*f_it).end(); ++f_jt)
			delete (*f_jt);
		(*f_it).clear();
	}
	feature_list_.clear();


	for (unsigned int cuboid_index = 0; true; ++cuboid_index)
	{
		std::stringstream sstr;
		sstr << _filename_prefix << cuboid_index << std::string(".csv");
		std::string attributes_filename = sstr.str();

		QFileInfo attributes_file(attributes_filename.c_str());
		if (!attributes_file.exists())
			break;

		std::cout << "Loading '" << attributes_filename << "'..." << std::endl;

		std::list<MeshCuboidFeatures *> stats;
		MeshCuboidFeatures::load_feature_collection(
			attributes_filename.c_str(), stats);

		feature_list_.push_back(stats);
	}

	return true;
}

bool MeshCuboidTrainer::load_transformations(const std::string &_filename_prefix)
{
	for (std::vector< std::list<MeshCuboidTransformation *> >::iterator t_it = transformation_list_.begin();
		t_it != transformation_list_.end(); ++t_it)
	{
		for (std::list<MeshCuboidTransformation *>::iterator t_jt = (*t_it).begin();
			t_jt != (*t_it).end(); ++t_jt)
			delete (*t_jt);
		(*t_it).clear();
	}
	transformation_list_.clear();


	for (unsigned int cuboid_index = 0; true; ++cuboid_index)
	{
		std::stringstream  transformation_filename_sstr;
		transformation_filename_sstr << _filename_prefix << cuboid_index << std::string(".csv");

		QFileInfo transformation_file(transformation_filename_sstr.str().c_str());
		if (!transformation_file.exists())
			break;

		std::cout << "Loading '" << transformation_filename_sstr.str() << "'..." << std::endl;

		std::list<MeshCuboidTransformation *> stats;
		MeshCuboidTransformation::load_transformation_collection(
			transformation_filename_sstr.str().c_str(), stats);

		transformation_list_.push_back(stats);
	}

	return true;
}

void MeshCuboidTrainer::get_joint_normal_relations(
	std::vector< std::vector<MeshCuboidJointNormalRelations *> > &_relations,
	const std::list<std::string> *_ignored_object_list) const
{
	const unsigned int num_features = MeshCuboidFeatures::k_num_features;

	unsigned int num_labels = feature_list_.size();
	assert(transformation_list_.size() == num_labels);

	for (std::vector< std::vector<MeshCuboidJointNormalRelations *> >::iterator it_1 = _relations.begin(); it_1 != _relations.end(); ++it_1)
		for (std::vector<MeshCuboidJointNormalRelations *>::iterator it_2 = (*it_1).begin(); it_2 != (*it_1).end(); ++it_2)
			delete (*it_2);

	_relations.clear();
	_relations.resize(num_labels);
	for (unsigned int cuboid_index = 0; cuboid_index < num_labels; ++cuboid_index)
		_relations[cuboid_index].resize(num_labels, NULL);


	for (unsigned int label_index_1 = 0; label_index_1 < num_labels; ++label_index_1)
	{
		for (unsigned int label_index_2 = 0; label_index_2 < num_labels; ++label_index_2)
		{
			if (label_index_1 == label_index_2) continue;

			// NOTE:
			// 'object_list_' should contain all object names.
			assert(object_list_.size() == feature_list_[label_index_1].size());
			assert(object_list_.size() == feature_list_[label_index_2].size());
			assert(object_list_.size() == transformation_list_[label_index_1].size());
			assert(object_list_.size() == transformation_list_[label_index_2].size());

			std::vector<MeshCuboidFeatures *> feature_1;
			std::vector<MeshCuboidFeatures *> feature_2;
			std::vector<MeshCuboidTransformation *> transformation_1;
			std::vector<MeshCuboidTransformation *> transformation_2;

			feature_1.reserve(feature_list_[label_index_1].size());
			feature_2.reserve(feature_list_[label_index_2].size());
			transformation_1.reserve(transformation_list_[label_index_1].size());
			transformation_2.reserve(transformation_list_[label_index_2].size());

			std::list<std::string>::const_iterator o_it = object_list_.begin();
			std::list<MeshCuboidFeatures *>::const_iterator f_it_1 = feature_list_[label_index_1].begin();
			std::list<MeshCuboidFeatures *>::const_iterator f_it_2 = feature_list_[label_index_2].begin();
			std::list<MeshCuboidTransformation *>::const_iterator t_it_1 = transformation_list_[label_index_1].begin();
			std::list<MeshCuboidTransformation *>::const_iterator t_it_2 = transformation_list_[label_index_2].begin();

			int num_objects = 0;
			while (true)
			{
				if (f_it_1 == feature_list_[label_index_1].end()
					|| f_it_2 == feature_list_[label_index_2].end()
					|| t_it_1 == transformation_list_[label_index_1].end()
					|| t_it_2 == transformation_list_[label_index_2].end())
					break;

				bool has_values = (!(*f_it_1)->has_nan() && !(*f_it_2)->has_nan());

				if (has_values && _ignored_object_list)
				{
					// Check whether the current object should be ignored.
					for (std::list<std::string>::const_iterator io_it = _ignored_object_list->begin();
						io_it != _ignored_object_list->end(); ++io_it)
					{
						if ((*o_it) == (*io_it))
						{
							std::cout << "Mesh [" << (*o_it) << "] is ignored." << std::endl;
							has_values = false;
							break;
						}
					}
				}

				if (has_values)
				{
					feature_1.push_back(*f_it_1);
					feature_2.push_back(*f_it_2);
					transformation_1.push_back(*t_it_1);
					transformation_2.push_back(*t_it_2);
					++num_objects;
				}

				++o_it;
				++f_it_1;
				++f_it_2;
				++t_it_1;
				++t_it_2;
			}

			if (num_objects == 0) continue;


			_relations[label_index_1][label_index_2] = new MeshCuboidJointNormalRelations();
			MeshCuboidJointNormalRelations *relation_12 = _relations[label_index_1][label_index_2];
			assert(relation_12);

			assert(feature_1.size() == num_objects);
			assert(feature_2.size() == num_objects);
			assert(transformation_1.size() == num_objects);
			assert(transformation_2.size() == num_objects);

			Eigen::MatrixXd X_1(num_objects, num_features);
			Eigen::MatrixXd X_2(num_objects, num_features);

			for (int object_index = 0; object_index < num_objects; ++object_index)
			{
				assert(feature_1[object_index]);
				assert(feature_2[object_index]);
				assert(transformation_1[object_index]);
				assert(transformation_2[object_index]);

				Eigen::VectorXd transformed_feature_1 = transformation_2[object_index]->get_transformed_features(
					(*feature_1[object_index]));

				Eigen::VectorXd transformed_feature_2 = transformation_1[object_index]->get_transformed_features(
					(*feature_2[object_index]));

				assert(transformed_feature_1.size() == MeshCuboidFeatures::k_num_features);
				assert(transformed_feature_2.size() == MeshCuboidFeatures::k_num_features);
				
				X_1.row(object_index) = transformed_feature_1.transpose();
				X_2.row(object_index) = transformed_feature_2.transpose();
			}

			Eigen::MatrixXd X(num_objects, 2 * num_features);
			X << X_1, X_2;

			Eigen::RowVectorXd mean = X.colwise().mean();
			Eigen::MatrixXd centered_X = X.rowwise() - mean;

			Eigen::MatrixXd cov = (centered_X.transpose() * centered_X) / static_cast<double>(num_objects);
			Eigen::MatrixXd inv_cov = regularized_inverse(cov);

			relation_12->set_mean(mean.transpose());
			relation_12->set_inv_cov(inv_cov);


#ifdef DEBUG_TEST
			Eigen::MatrixXd diff = (X.rowwise() - mean).transpose();
			Eigen::VectorXd error = (diff.transpose() * inv_cov * diff).diagonal();
			std::cout << "(" << label_index_1 << ", " << label_index_2 << "): max_error = " << error.maxCoeff() << std::endl;
#endif
		}
	}
}

void MeshCuboidTrainer::get_cond_normal_relations(
	std::vector< std::vector<MeshCuboidCondNormalRelations *> > &_relations,
	const std::list<std::string> *_ignored_object_list) const
{
	const unsigned int num_features = MeshCuboidFeatures::k_num_features;
	const unsigned int num_global_feature_values = MeshCuboidFeatures::k_num_global_feature_values;

	unsigned int num_labels = feature_list_.size();
	assert(transformation_list_.size() == num_labels);

	for (std::vector< std::vector<MeshCuboidCondNormalRelations *> >::iterator it_1 = _relations.begin(); it_1 != _relations.end(); ++it_1)
		for (std::vector<MeshCuboidCondNormalRelations *>::iterator it_2 = (*it_1).begin(); it_2 != (*it_1).end(); ++it_2)
			delete (*it_2);

	_relations.clear();
	_relations.resize(num_labels);
	for (unsigned int cuboid_index = 0; cuboid_index < num_labels; ++cuboid_index)
		_relations[cuboid_index].resize(num_labels, NULL);


	for (unsigned int label_index_1 = 0; label_index_1 < num_labels; ++label_index_1)
	{
		for (unsigned int label_index_2 = 0; label_index_2 < num_labels; ++label_index_2)
		{
			if (label_index_1 == label_index_2) continue;

			// NOTE:
			// 'object_list_' should contain all object names.
			assert(object_list_.size() == feature_list_[label_index_1].size());
			assert(object_list_.size() == feature_list_[label_index_2].size());
			assert(object_list_.size() == transformation_list_[label_index_1].size());
			assert(object_list_.size() == transformation_list_[label_index_2].size());

			std::vector<MeshCuboidFeatures *> feature_1;
			std::vector<MeshCuboidFeatures *> feature_2;
			std::vector<MeshCuboidTransformation *> transformation_1;

			feature_1.reserve(feature_list_[label_index_1].size());
			feature_2.reserve(feature_list_[label_index_2].size());
			transformation_1.reserve(transformation_list_[label_index_1].size());

			std::list<std::string>::const_iterator o_it = object_list_.begin();
			std::list<MeshCuboidFeatures *>::const_iterator f_it_1 = feature_list_[label_index_1].begin();
			std::list<MeshCuboidFeatures *>::const_iterator f_it_2 = feature_list_[label_index_2].begin();
			std::list<MeshCuboidTransformation *>::const_iterator t_it_1 = transformation_list_[label_index_1].begin();

			int num_objects = 0;
			while (true)
			{
				if (f_it_1 == feature_list_[label_index_1].end()
					|| f_it_2 == feature_list_[label_index_2].end()
					|| t_it_1 == transformation_list_[label_index_1].end())
					break;

				bool has_values = (!(*f_it_1)->has_nan() && !(*f_it_2)->has_nan());

				if (has_values && _ignored_object_list)
				{
					// Check whether the current object should be ignored.
					for (std::list<std::string>::const_iterator io_it = _ignored_object_list->begin();
						io_it != _ignored_object_list->end(); ++io_it)
					{
						if ((*o_it) == (*io_it))
						{
							std::cout << "Mesh [" << (*o_it) << "] is ignored." << std::endl;
							has_values = false;
							break;
						}
					}
				}

				if (has_values)
				{
					feature_1.push_back(*f_it_1);
					feature_2.push_back(*f_it_2);
					transformation_1.push_back(*t_it_1);
					++num_objects;
				}

				++o_it;
				++f_it_1;
				++f_it_2;
				++t_it_1;
			}

			if (num_objects == 0) continue;


			_relations[label_index_1][label_index_2] = new MeshCuboidCondNormalRelations();
			MeshCuboidCondNormalRelations *relation_12 = _relations[label_index_1][label_index_2];
			assert(relation_12);

			assert(feature_1.size() == num_objects);
			assert(feature_2.size() == num_objects);
			assert(transformation_1.size() == num_objects);

			Eigen::MatrixXd X_1(num_objects, num_global_feature_values);
			Eigen::MatrixXd X_2(num_objects, num_features);

			for (int object_index = 0; object_index < num_objects; ++object_index)
			{
				assert(feature_1[object_index]);
				assert(feature_2[object_index]);
				assert(transformation_1[object_index]);

				X_1.row(object_index) = feature_1[object_index]->get_features().bottomRows(num_global_feature_values);

				Eigen::VectorXd transformed_feature_2 = transformation_1[object_index]->get_transformed_features(
					(*feature_2[object_index]));

				assert(transformed_feature_2.size() == MeshCuboidFeatures::k_num_features);

				X_2.row(object_index) = transformed_feature_2.transpose();
			}

			Eigen::RowVectorXd mean_1 = X_1.colwise().mean();
			Eigen::RowVectorXd mean_2 = X_2.colwise().mean();


			//Eigen::MatrixXd centered_X_1 = X_1.rowwise() - mean_1;
			//Eigen::MatrixXd centered_X_2 = X_2.rowwise() - mean_2;

			//Eigen::MatrixXd cov_11 = (centered_X_1.transpose() * centered_X_1) / static_cast<double>(num_objects);
			//Eigen::MatrixXd cov_22 = (centered_X_2.transpose() * centered_X_2) / static_cast<double>(num_objects);
			//Eigen::MatrixXd cov_12 = (centered_X_1.transpose() * centered_X_2) / static_cast<double>(num_objects);
			//Eigen::MatrixXd cov_21 = (centered_X_2.transpose() * centered_X_1) / static_cast<double>(num_objects);
			//Eigen::MatrixXd inv_cov_11 = regularized_inverse(cov_11);

			//Eigen::MatrixXd conditional_mean_A = cov_21 * inv_cov_11;
			//Eigen::VectorXd conditional_mean_b = mean_2.transpose() - conditional_mean_A * mean_1.transpose();

			//Eigen::MatrixXd conditional_cov_21 = cov_22 - cov_21 * inv_cov_11 * cov_12;
			//Eigen::MatrixXd conditional_inv_cov_21 = regularized_inverse(conditional_cov_21);


			// http://www.rni.helsinki.fi/~jmh/mrf08/helsinki-1.pdf, page 41.
			Eigen::MatrixXd X(num_objects, num_global_feature_values + num_features);
			X << X_1, X_2;

			Eigen::RowVectorXd mean = X.colwise().mean();
			Eigen::MatrixXd centered_X = X.rowwise() - mean;

			Eigen::MatrixXd cov = (centered_X.transpose() * centered_X) / static_cast<double>(num_objects);
			Eigen::MatrixXd inv_cov = regularized_inverse(cov);

			Eigen::MatrixXd inv_cov_22 = inv_cov.block(
				num_global_feature_values, num_global_feature_values, num_features, num_features);

			Eigen::MatrixXd inv_cov_21 = inv_cov.block(
				num_global_feature_values, 0, num_features, num_global_feature_values);

			Eigen::MatrixXd conditional_mean_A = regularized_inverse(inv_cov_22) * inv_cov_21;
			Eigen::VectorXd conditional_mean_b = mean_2.transpose() - conditional_mean_A * mean_1.transpose();
			Eigen::MatrixXd conditional_inv_cov_21 = inv_cov_22;

			relation_12->set_mean_A(conditional_mean_A);
			relation_12->set_mean_b(conditional_mean_b);
			relation_12->set_inv_cov(conditional_inv_cov_21);


#ifdef DEBUG_TEST
			Eigen::MatrixXd mean_12 = (conditional_mean_A * X_1.transpose()).colwise() + conditional_mean_b;
			Eigen::MatrixXd diff = (X_2.transpose() - mean_12);
			Eigen::VectorXd error = (diff.transpose() * conditional_inv_cov_21 * diff).diagonal();
			std::cout << "(" << label_index_1 << ", " << label_index_2 << "): max_error = " << error.maxCoeff() << std::endl;
#endif
		}
	}
}

void MeshCuboidTrainer::get_label_cooccurrences(std::vector< std::list<LabelIndex> > &_cooccurrence_labels)const
{
	unsigned int num_labels = feature_list_.size();
	assert(transformation_list_.size() == num_labels);

	_cooccurrence_labels.clear();
	_cooccurrence_labels.resize(num_labels);

	for (unsigned int label_index_1 = 0; label_index_1 < num_labels; ++label_index_1)
	{
		for (unsigned int label_index_2 = 0; label_index_2 < num_labels; ++label_index_2)
		{
			if (label_index_1 == label_index_2) continue;

			// NOTE:
			// 'object_list_' should contain all object names.
			assert(object_list_.size() == feature_list_[label_index_1].size());
			assert(object_list_.size() == feature_list_[label_index_2].size());

			std::list<MeshCuboidFeatures *>::const_iterator f_it_1 = feature_list_[label_index_1].begin();
			std::list<MeshCuboidFeatures *>::const_iterator f_it_2 = feature_list_[label_index_2].begin();

			int num_objects = 0;
			while (true)
			{
				if (f_it_1 == feature_list_[label_index_1].end()
					|| f_it_2 == feature_list_[label_index_2].end())
					break;

				bool has_values = (!(*f_it_1)->has_nan() && !(*f_it_2)->has_nan());

				if (has_values)
					++num_objects;

				++f_it_1;
				++f_it_2;
			}

			// NOTE: If both labels appear simultaneously at least in one object,
			// they are defined as co-occurred labels.
			if (num_objects > 0)
				_cooccurrence_labels[label_index_1].push_back(label_index_2);
		}
	}
}

void MeshCuboidTrainer::get_missing_label_index_groups(
	const std::list<LabelIndex> &_given_label_indices,
	std::list< std::list<LabelIndex> > &_missing_label_index_groups)const
{
	unsigned int num_labels = feature_list_.size();
	assert(transformation_list_.size() == num_labels);

	std::vector< std::list<LabelIndex> > cooccurrence_labels;
	get_label_cooccurrences(cooccurrence_labels);
	assert(cooccurrence_labels.size() == num_labels);


	// Consider co-occurring labels of the given label indices.
	bool *is_label_missing = new bool[num_labels];
	memset(is_label_missing, true, num_labels*sizeof(bool));

	for (std::list<LabelIndex>::const_iterator it = _given_label_indices.begin();
		it != _given_label_indices.end(); ++it)
	{
		LabelIndex curr_label_index = (*it);
		assert(curr_label_index < num_labels);

		bool *is_label_cooccurred = new bool[num_labels];
		memset(is_label_cooccurred, false, num_labels*sizeof(bool));
		is_label_cooccurred[curr_label_index] = true;

		const std::list<LabelIndex> &curr_cooccurrence_labels = cooccurrence_labels[curr_label_index];
		for (std::list<LabelIndex>::const_iterator n_it = curr_cooccurrence_labels.begin();
			n_it != curr_cooccurrence_labels.end(); ++n_it)
		{
			LabelIndex neighbor_label_index = (*n_it);
			assert(neighbor_label_index < num_labels);
			is_label_cooccurred[neighbor_label_index] = true;
		}

		// Ignore non-co-occurred labels.
		for (LabelIndex label_index = 0; label_index < num_labels; ++label_index)
			if (!is_label_cooccurred[label_index])
				is_label_missing[label_index] = false;

		// Ignore existing labels.
		is_label_missing[curr_label_index] = false;

		delete[] is_label_cooccurred;
	}


	// Clustering missing label indices.
	_missing_label_index_groups.clear();

	while (true)
	{
		LabelIndex seed_label_index = 0;
		for (; seed_label_index < num_labels; ++seed_label_index)
			if (is_label_missing[seed_label_index])
				break;

		if (seed_label_index == num_labels) break;

		std::deque<LabelIndex> queue;
		queue.push_back(seed_label_index);
		is_label_missing[seed_label_index] = false;


		std::list<LabelIndex> missing_label_indices;
		while (!queue.empty())
		{
			LabelIndex curr_label_index = queue.front();
			assert(curr_label_index < num_labels);
			missing_label_indices.push_back(curr_label_index);
			queue.pop_front();

			const std::list<LabelIndex> &curr_cooccurrence_labels = cooccurrence_labels[curr_label_index];
			for (std::list<LabelIndex>::const_iterator n_it = curr_cooccurrence_labels.begin();
				n_it != curr_cooccurrence_labels.end(); ++n_it)
			{
				LabelIndex neighbor_label_index = (*n_it);
				assert(neighbor_label_index < num_labels);

				if (is_label_missing[neighbor_label_index])
				{
					queue.push_back(neighbor_label_index);
					is_label_missing[neighbor_label_index] = false;
				}
			}
		}

		_missing_label_index_groups.push_back(missing_label_indices);
	}

	delete[] is_label_missing;
}

void MeshCuboidTrainer::load_joint_normal_relations(
	const unsigned int _num_labels, const std::string _filename_prefix,
	std::vector< std::vector<MeshCuboidJointNormalRelations *> > &_relations)
{
	for (std::vector< std::vector<MeshCuboidJointNormalRelations *> >::iterator it_1 = _relations.begin(); it_1 != _relations.end(); ++it_1)
		for (std::vector<MeshCuboidJointNormalRelations *>::iterator it_2 = (*it_1).begin(); it_2 != (*it_1).end(); ++it_2)
			delete (*it_2);

	_relations.clear();
	_relations.resize(_num_labels);

	for (LabelIndex label_index_1 = 0; label_index_1 < _num_labels; ++label_index_1)
	{
		_relations[label_index_1].resize(_num_labels, NULL);
		for (LabelIndex label_index_2 = 0; label_index_2 < _num_labels; ++label_index_2)
		{
			if (label_index_1 == label_index_2)
				continue;

			std::stringstream relation_filename_sstr;
			relation_filename_sstr << _filename_prefix
				<< label_index_1 << std::string("_")
				<< label_index_2 << std::string(".csv");

			QFileInfo relation_file(relation_filename_sstr.str().c_str());
			if (!relation_file.exists()) continue;

			_relations[label_index_1][label_index_2] = new MeshCuboidJointNormalRelations();
			bool ret = _relations[label_index_1][label_index_2]->load_joint_normal_csv(
				relation_filename_sstr.str().c_str());

			if (!ret)
			{
				do {
					std::cout << '\n' << "Press the Enter key to continue.";
				} while (std::cin.get() != '\n');
			}
		}
	}
}

void MeshCuboidTrainer::load_cond_normal_relations(
	const unsigned int _num_labels, const std::string _filename_prefix,
	std::vector< std::vector<MeshCuboidCondNormalRelations *> > &_relations)
{
	for (std::vector< std::vector<MeshCuboidCondNormalRelations *> >::iterator it_1 = _relations.begin(); it_1 != _relations.end(); ++it_1)
		for (std::vector<MeshCuboidCondNormalRelations *>::iterator it_2 = (*it_1).begin(); it_2 != (*it_1).end(); ++it_2)
			delete (*it_2);

	_relations.clear();
	_relations.resize(_num_labels);

	for (LabelIndex label_index_1 = 0; label_index_1 < _num_labels; ++label_index_1)
	{
		_relations[label_index_1].resize(_num_labels, NULL);
		for (LabelIndex label_index_2 = 0; label_index_2 < _num_labels; ++label_index_2)
		{
			if (label_index_1 == label_index_2)
				continue;

			std::stringstream relation_filename_sstr;
			relation_filename_sstr << std::string("")
				<< label_index_1 << std::string("_")
				<< label_index_2 << std::string(".csv");

			QFileInfo relation_file(relation_filename_sstr.str().c_str());
			if (!relation_file.exists()) continue;

			_relations[label_index_1][label_index_2] = new MeshCuboidCondNormalRelations();
			bool ret = _relations[label_index_1][label_index_2]->load_cond_normal_csv(
				relation_filename_sstr.str().c_str());
			if (!ret)
			{
				do {
					std::cout << '\n' << "Press the Enter key to continue.";
				} while (std::cin.get() != '\n');
			}
		}
	}
	
}
